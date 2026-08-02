# Sol Engine — LLM Session Starter

> Copy-paste this at the start of any LLM coding session.

Read these files in order before doing anything else:

1. docs/llm.md — conventions, rules, anti-goals, operations
2. docs/llms.md — curated index of all documentation
3. docs/specs/roadmap.md — what's done and what's next

Then, depending on the task, read the relevant subsystem spec:
- UI work → docs/specs/scene_spec.md + src/sol/scene/*.h
- Audio work → docs/specs/neptune_spec.md + src/sol/audio/*.h
- Build work → docs/specs/build_spec.md + scripts/build.py

When your task is complete:
- Update roadmap status markers if you finished anything
- Update the relevant spec if architecture/API changed
- Update docs/wiki/file_index.md if you added or renamed files
- Flag any code-vs-spec contradictions you noticed
