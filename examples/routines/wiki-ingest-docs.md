---
name: wiki-ingest-docs
schedule: manual
tools: wiki_lookup, bash, web_fetch
notify: true
enabled: true
---
Today is {{date}}.

Ingest useful documentation into the project wiki. For each URL below,
use `bash` to run `pluto wiki ingest <url>` and report the result.

URLs to ingest:
{{args}}

If no URLs are provided in {{args}}, scan the project for a README or
documentation directory and suggest URLs that would be valuable to ingest
(API docs, framework guides, library references used by the project).

After ingestion, run `pluto wiki list` to show the updated wiki contents.
