# codefacer
This is an auto tool for [siemens/codeface] using the local GitLab repository 
[siemens/codeface]: https://github.com/siemens/codeface

* Setup

  Run `make install` to install codefacer. Edit /etc/codefacer.conf to fit your local environment.
  Supposing that you setup GitLab with creating an account `git`, type`sudo -u git -H codefacer` to use codeface to analyze the projects in the local GitLab repository.

* Function

  Codefacer will create a conf file for each project, using the git tag information to figure out the revision and rcs list, setting tagging `committer2author`.
  If a project has no tag, it uses the method translate from
  ```
  codeface/codeface/util.py
  generate_analysis_windows(repo, window_size_months)
  ```
  to create revisions, with a two week window.
  To distinguish each project, Codefacer uses `username@path` as the project name.
  After each configure file created, call Codeface.
  
  To use Codeface to analyze a project under a .git folder, it is necessary to replace 
  ```
  repo = pathjoin(gitdir, conf["repo"], ".git")
  ```
  with
  ```
  repo = pathjoin(gitdir, conf["repo"])
  ```
  in codeface/project.py.
