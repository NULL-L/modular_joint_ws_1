
(cl:in-package :asdf)

(defsystem "can_test_02-msg"
  :depends-on (:roslisp-msg-protocol :roslisp-utils )
  :components ((:file "_package")
    (:file "UpToDown" :depends-on ("_package_UpToDown"))
    (:file "_package_UpToDown" :depends-on ("_package"))
    (:file "tem" :depends-on ("_package_tem"))
    (:file "_package_tem" :depends-on ("_package"))
    (:file "vel" :depends-on ("_package_vel"))
    (:file "_package_vel" :depends-on ("_package"))
    (:file "DownToUp" :depends-on ("_package_DownToUp"))
    (:file "_package_DownToUp" :depends-on ("_package"))
    (:file "orig_new" :depends-on ("_package_orig_new"))
    (:file "_package_orig_new" :depends-on ("_package"))
    (:file "orig" :depends-on ("_package_orig"))
    (:file "_package_orig" :depends-on ("_package"))
    (:file "recv" :depends-on ("_package_recv"))
    (:file "_package_recv" :depends-on ("_package"))
  ))