// Tool: wiki_lookup — search the project wiki knowledge base.

#include "WikiLookup.h"
#include "../wiki.h"
#include "../json.h"

#include <string>

namespace pluto {

namespace {

class WikiLookupTool : public Tool {
public:
	std::string name() const override { return "wiki_lookup"; }

	std::string description() const override {
		return "Search the project wiki knowledge base. Returns relevant "
		       "entries from the LLM-curated wiki (markdown files with "
		       "topic, tags, and structured content). Use this to find "
		       "project-specific knowledge, API patterns, architecture "
		       "decisions, or any previously ingested documentation.";
	}

	std::string input_schema_json() const override {
		return R"JSON({
  "type": "object",
  "properties": {
    "query": {
      "type": "string",
      "description": "Search query. Can include topic names, tags, or natural language descriptions."
    },
    "top_k": {
      "type": "integer",
      "description": "Number of results to return (1-5). Default: 3."
    }
  },
  "required": ["query"]
})JSON";
	}

	std::string preview(const std::string& input_json) const override {
		try {
			json::Value v = json::Value::parse(input_json);
			return "wiki_lookup(" + v.str("query") + ")";
		} catch (...) {
			return "wiki_lookup(?)";
		}
	}

	ToolResult execute(const std::string& input_json) override {
		ToolResult tr;
		try {
			json::Value v = json::Value::parse(input_json);
			std::string query = v.str("query");
			if (query.empty()) {
				tr.is_error = true;
				tr.output = "missing required parameter: query";
				return tr;
			}

			int top_k = 3;
			const json::Value* tk = v.find("top_k");
			if (tk && tk->is_num()) {
				top_k = (int)tk->as_num();
				if (top_k < 1) top_k = 1;
				if (top_k > 5) top_k = 5;
			}

			if (WikiEntryCount() == 0) {
				tr.is_error = true;
				tr.output = "Wiki not loaded. No .md files found in wiki directory.";
				return tr;
			}

			auto results = WikiSearch(query, top_k);
			if (results.empty()) {
				tr.output = "No matching wiki entries for: " + query;
				return tr;
			}

			for (size_t i = 0; i < results.size(); ++i) {
				const auto& r = results[i];
				if (i > 0) tr.output += "\n---\n\n";
				tr.output += "## " + r.entry->topic + "\n";
				if (!r.entry->tags.empty()) {
					tr.output += "Tags:";
					for (const auto& t : r.entry->tags)
						tr.output += " " + t;
					tr.output += "\n";
				}
				if (!r.entry->source.empty())
					tr.output += "Source: " + r.entry->source + "\n";
				tr.output += "\n" + r.entry->body + "\n";
			}
			return tr;
		} catch (const std::exception& e) {
			tr.is_error = true;
			tr.output = std::string("invalid input: ") + e.what();
			return tr;
		}
	}
};

}  // namespace

std::unique_ptr<Tool> MakeWikiLookupTool() {
	return std::make_unique<WikiLookupTool>();
}

}  // namespace pluto
