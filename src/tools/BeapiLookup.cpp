// Tool: beapi_lookup — search the local BeAPI reference for code snippets.

#include "BeapiLookup.h"
#include "../rag.h"
#include "../json.h"

#include <string>

namespace pluto {

namespace {

class BeapiLookupTool : public Tool {
public:
	std::string name() const override { return "beapi_lookup"; }

	std::string description() const override {
		return "Search the local BeAPI code reference (390 snippets covering "
		       "202 Be classes). Returns up to 3 relevant code examples with "
		       "topic, classes used, and complete code. Use this when writing "
		       "Haiku-native C++ code to find correct API usage patterns.";
	}

	std::string input_schema_json() const override {
		return R"JSON({
  "type": "object",
  "properties": {
    "query": {
      "type": "string",
      "description": "Search query. Can include Be class names (BView, BSlider, BMessage), API concepts (layout, draw, attribute), or natural language descriptions."
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
			return "beapi_lookup(" + v.str("query") + ")";
		} catch (...) {
			return "beapi_lookup(?)";
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

			if (RagEntryCount() == 0) {
				tr.is_error = true;
				tr.output = "BeAPI index not loaded. Ensure beapi-index.jsonl exists.";
				return tr;
			}

			auto results = RagSearch(query, top_k);
			if (results.empty()) {
				tr.output = "No matching BeAPI snippets for: " + query;
				return tr;
			}

			for (size_t i = 0; i < results.size(); ++i) {
				const auto& r = results[i];
				if (i > 0) tr.output += "\n---\n\n";
				tr.output += "## " + r.entry->topic + "\n";
				if (!r.entry->classes.empty()) {
					tr.output += "Classes:";
					for (const auto& c : r.entry->classes)
						tr.output += " " + c;
					tr.output += "\n";
				}
				tr.output += "\n" + r.entry->answer + "\n";
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

std::unique_ptr<Tool> MakeBeapiLookupTool() {
	return std::make_unique<BeapiLookupTool>();
}

}  // namespace pluto
