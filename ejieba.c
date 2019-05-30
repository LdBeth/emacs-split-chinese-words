#include <lib/jieba.h>
#include <emacs-module.h>
#include <string.h>

/*
const char* DICT_PATH = "./dict/jieba.dict.utf8";
const char* HMM_PATH = "./dict/hmm_model.utf8";
const char* USER_DICT = "./dict/user.dict.utf8";
const char* IDF_PATH = "./dict/idf.utf8";
const char* STOP_WORDS_PATH = "./dict/stop_words.utf8";
*/

int plugin_is_GPL_compatible = 1;

struct Data {
  emacs_value t;
  emacs_value nil;
  emacs_value user_ptr;
};

/* Copy a string out of an emacs argument. Returns a new pointer to a string on
 * the heap. It's the user's responsibility to free this pointer. */
static const char*
copy_string_from_emacs(emacs_env *env, emacs_value arg)
{
  ptrdiff_t s = 1024;
  char *str = (char *)malloc(s), *new_str;
  if (str == NULL) {
    emacs_value signal = env->intern(env, "error");
    const char *s = "malloc() failed!";
    emacs_value msg = env->make_string(env, s, strlen(s));
    env->non_local_exit_signal(env, signal, msg);
    return NULL;
  }

  for (int i = 0; i < 2; i++) {
    if (env->copy_string_contents(env, arg, str, &s)) {
      return str;
    }

    /* The buffer wasn't big enough to capture the string from
     * emacs. Realloc and try again. */
    new_str = (char *)realloc(str, s);
    if (new_str == NULL) {
      free(str);
      emacs_value signal = env->intern(env, "error");
      const char *s = "realloc() failed!";
      emacs_value msg = env->make_string(env, s, strlen(s));
      env->non_local_exit_signal(env, signal, msg);
      return NULL;
    }
    str = new_str;  /* TODO: use reallocf */
  }

  return NULL;
}


static void
finalize_jieba(void *ptr)
{
  FreeJieba(ptr);
}



static emacs_value
Fmake_jieba(emacs_env *env, ptrdiff_t nargs,
                        emacs_value args[], void *t)
{
  const char *paths[5];
  for (int i = 0; i < 5; i++)
    {
      paths[i] = copy_string_from_emacs(env, args[i]);
    }

  /* FIXME: cjieba calls abort() upon failing loading dictionaries,
     +     which breaks emacs. */
  Jieba dict = NewJieba(paths[0], paths[1], paths[2], paths[3], paths[4]);

  for (int i = 0; i < 5; i++)
    {
      free((char*)paths[i]);
    }

  return env->make_user_ptr(env, &finalize_jieba, dict);
}


/* We check if a user ptr a Jieba instance by comparing it's
   finalizer to &finalize_jieba. */
static bool
is_jieba(emacs_env *env, emacs_value u_ptr, void *data)
{

  struct Data values = *((struct Data *) data);
  if (!env->eq (env, env->type_of(env, u_ptr),
                values.user_ptr)) {
    return false;
  } else {
    if (env->get_user_finalizer(env,u_ptr) == &finalize_jieba) {
      return true;
    } else {
      return false;
    }
  }
}

/* A Lisp wrapper */
static emacs_value
Fjiebap_ptr_p(emacs_env *env, ptrdiff_t nargs,
              emacs_value args[], void *data)
{
  struct Data values = *((struct Data *) data);
  if (is_jieba(env,args[0],data)) {
    return values.t;
  } else {
    return values.nil;
  }
}


static emacs_value
Finsert_user_word(emacs_env *env, ptrdiff_t nargs,
                  emacs_value args[], void *data)
{
  Jieba *handle = env->get_user_ptr(env, args[0]);
  if (!is_jieba(env, args[0], data)) {
    emacs_value signal = env->intern(env, "wrong-type-argument");
    env->non_local_exit_signal(env, signal, args[0]);
    return NULL;
  }
  const char *s = copy_string_from_emacs(env, args[1]);

  if (env->non_local_exit_check(env) != emacs_funcall_exit_return) {
    emacs_value signal = env->intern(env, "wrong-type-argument");
    env->non_local_exit_signal(env, signal, args[1]);
    return NULL;
  }

  struct Data values = *((struct Data *)data);
  if (JiebaInsertUserWord(handle, s)){
    return values.t;
  }else {
    return values.nil;
  }
}

static emacs_value
make_lisp_vector(emacs_env *env, emacs_value args[], size_t size)
{
  emacs_value Qvector = env->intern(env, "vector");
  return env->funcall(env, Qvector, size, args);
}

static emacs_value
Fsplit_words(emacs_env *env, ptrdiff_t nargs,
             emacs_value args[], void *data)
{
  Jieba *handle = env->get_user_ptr(env, args[0]);
  /* check is ARG1 a ptr of Jieba */
  if (!is_jieba(env, args[0], data)) {
    emacs_value signal = env->intern(env, "wrong-type-argument");
    env->non_local_exit_signal(env, signal, args[0]);
    return NULL;
  }

  const char *s = copy_string_from_emacs(env, args[1]);

  /* check is ARG2 of type string */
  if (env->non_local_exit_check(env) != emacs_funcall_exit_return) {
    emacs_value signal = env->intern(env, "wrong-type-argument");
    env->non_local_exit_signal(env, signal, args[1]);
    return NULL;
  }

  size_t len = strlen(s);
  CJiebaWord* words = Cut(handle, s, len);
  CJiebaWord* x;
  int i = 0;
  emacs_value list[len];
  for (x = words; x->word; x++) {
    list[i] = env->make_string(env,x->word,x->len);
    i++;
  }
  FreeWords(words);
  return make_lisp_vector(env, list, i);
}

static void
bind_function(emacs_env *env, const char *name, emacs_value Sfun)
{
  /* Set the function cell of the symbol named NAME to SFUN using
     the 'defalias' function.  */

  /* Convert the strings to symbols by interning them */
  emacs_value Qdefalias = env->intern(env, "defalias");
  emacs_value Qsym = env->intern(env, name);

  /* Prepare the arguments array */
  emacs_value args[] = { Qsym, Sfun };

  /* Make the call (2 == nb of arguments) */
  env->funcall(env, Qdefalias, 2, args);
}

void provide (emacs_env *env, const char *feature)
{
  emacs_value Qfeat = env->intern (env, feature);
  emacs_value Qprovide = env->intern (env, "provide");
  emacs_value args[] = { Qfeat };

  env->funcall (env, Qprovide, 1, args);
}

int
emacs_module_init (struct emacs_runtime *ert)
{
  emacs_env *env = ert->get_environment (ert);

  if (ert->size < sizeof (*ert))
    return 1;

  static struct Data values;
  values.t = env->intern(env, "t");
  values.nil = env->intern(env, "nil");
  values.user_ptr = env->intern(env, "user-ptr");


#define DEFUN(lsym, csym, amin, amax, doc, data)                        \
  bind_function(env, lsym,                                              \
                env->make_function (env, amin, amax, csym, doc, data))
  DEFUN("ejieba--make-jieba", Fmake_jieba, 5, 5,
        "Load dictionaries, then produce a Jieba instance. \n\
Calling this function with invalid dictionaries would crash Emacs!!", NULL);
  DEFUN("ejieba--split-words", Fsplit_words, 2, 2,
        "Split Words.", &values);
  DEFUN("ejieba--insert-user-word", Finsert_user_word, 2, 2,
        "Insert new word to Jieba. Return t if success, else nil.", &values);
  DEFUN("ejieba--jieba-ptr-p", Fjiebap_ptr_p, 1, 1,
        "Return t if ARG is a jieba", &values);
#undef DEFUN

  provide(env, "ejieba");
  return 0;
}

