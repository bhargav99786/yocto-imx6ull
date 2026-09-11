# Agent Rules

## Conversation History & Context Retrieval
- Always review and take into account previous conversation history, past context, prior conversation summaries, and earlier task steps when responding to user requests.
- When full or earlier context is needed, actively retrieve and inspect past conversation logs and transcripts located under `<appDataDir>/brain/<conversation-id>/.system_generated/logs/transcript.jsonl` or past conversation summaries.
- Integrate details from prior turns, previous sessions, and transcript logs to maintain full continuity and avoid repeating previously resolved steps or re-asking for information.

## Build & Verification
- Always execute relevant build and test verification checks to empirically confirm changes before declaring a task completed.


