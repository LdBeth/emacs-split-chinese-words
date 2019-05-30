(require 'ejieba)

(defvar ejieba-dictionary)

(defvar ejieba-dictionary-path)

(defun ejieba-load-dictionary ()
  (let ((dicts (mapcar (lambda (s)
                         (expand-file-name s ejieba-dictionary-path))
                       '("jieba.dict.utf8" "hmm_model.utf8"
                         "user.dict.utf8"
                         "idf.utf8"
                         "stop_words.utf8"))))
    (when (reduce (lambda (a b) (and (file-exists-p a) b))
                  (append dicts '(t)))

      (setq ejieba-dictionary-path
            (apply 'ejieba--make-jieba dicts)))))

(defun ejieba-split-words (string)
  (ejieba--split-words ejieba-dictionary string))

(defun ejieba-jieba-ptr-p (o)
  (ejieba--jieba-ptr-p o))

(defun ejieba-insert-user-word (word)
  (ejieba--insert-user-word ejieba-dictionary word))

(provide 'ejieba-api)
