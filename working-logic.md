- user types required changes and fixes into "change-requirements.md"
- **commit the requirements file as filed, on its own, before converting anything.** Emptying it in
  the same commit that adds the tasks means the bullets were never committed as an addition and
  they vanish from the history entirely. Two commits: "file X as received", then the conversion.
  `git add -A` while the file holds unconverted bullets is the same mistake in milder form, so
  stage the requirements file on its own the moment new bullets appear.
- these change requirements are converted into detailed tasks and typed into "tasks.md", removing
  the contents of the previous file. **Every bullet becomes a task, or a line in `tasks.md` saying
  why it does not.** Count them: a conversion that turns three bullets into two tasks has silently
  dropped a requirement, and emptying the file is what makes it unrecoverable.
- each task carries the model that should implement it -- opus for the delicate and the
  architectural, sonnet for the mechanical and the fully specified.
- changes are implemented by either indicated agent, and progressively removed from tasks
- documentation is kept succinct; it exists to reduce token usage, not to be complete
- tasks and changes go into git along with the code, so the diffs of these two files are the
  record of what was asked and what was done
- Continuing the work means:
    - if there are change requirements: Opus converts them to tasks
    - if there are tasks: perform them with the indicated model
    - if there is nothing left: hunt bugs, or make the code, wikis and agent setup leaner
