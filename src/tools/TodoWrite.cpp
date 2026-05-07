#include "Builtin.h"
#include "Tool.h"
#include "../json.h"

#include <string>
#include <vector>

namespace pluto {

namespace {

struct TodoItem {
	std::string id;
	std::string content;
	std::string status;  // "pending", "in_progress", "completed"
	int priority;        // 0 = normal, 1 = high, 2 = critical
};

// Session-local todo list (one per tool instance, lives as long as the
// conversation tab / CLI session).
class TodoWriteTool : public Tool {
public:
	std::string name() const override { return "todo_write"; }

	std::string description() const override {
		return "Manage a task list for tracking progress on multi-step work. "
		       "Use this to plan complex tasks, track what is done, and show "
		       "the user your progress. Each item has an id, content, status "
		       "(pending/in_progress/completed), and priority (0=normal, "
		       "1=high, 2=critical). Pass the full desired list each time — "
		       "it replaces the previous list.";
	}

	std::string input_schema_json() const override {
		return R"({"type":"object","properties":{)"
		       R"("todos":{"type":"array","items":{"type":"object","properties":{)"
		       R"("id":{"type":"string","description":"Short unique identifier, e.g. '1' or 'build-fix'"},)"
		       R"("content":{"type":"string","description":"Task description"},)"
		       R"("status":{"type":"string","enum":["pending","in_progress","completed"],"description":"Current status"},)"
		       R"("priority":{"type":"integer","enum":[0,1,2],"description":"0=normal, 1=high, 2=critical"})"
		       R"(},"required":["id","content","status"]}}},"required":["todos"]})";
	}

	std::string preview(const std::string& input_json) const override {
		try {
			auto v = json::Value::parse(input_json);
			const auto* arr = v.find("todos");
			if (arr && arr->is_arr()) {
				int total = static_cast<int>(arr->as_arr().size());
				int done = 0;
				for (const auto& item : arr->as_arr()) {
					if (item.str("status") == "completed") ++done;
				}
				return "todo_write " + std::to_string(done) + "/"
				       + std::to_string(total) + " completed";
			}
			return "todo_write (empty)";
		} catch (...) { return Tool::preview(input_json); }
	}

	ToolResult execute(const std::string& input_json) override {
		ToolResult r;
		try {
			auto v = json::Value::parse(input_json);
			const auto* arr = v.find("todos");
			if (!arr || !arr->is_arr()) {
				r.is_error = true;
				r.output = "\"todos\" must be an array";
				return r;
			}

			fTodos.clear();
			for (const auto& item : arr->as_arr()) {
				if (!item.is_obj()) {
					r.is_error = true;
					r.output = "each todo must be an object";
					return r;
				}
				TodoItem t;
				t.id = item.str("id");
				t.content = item.str("content");
				t.status = item.str("status");
				if (t.id.empty() || t.content.empty() || t.status.empty()) {
					r.is_error = true;
					r.output = "each todo requires id, content, and status";
					return r;
				}
				if (t.status != "pending" && t.status != "in_progress"
					&& t.status != "completed") {
					r.is_error = true;
					r.output = "status must be pending, in_progress, or completed";
					return r;
				}
				const auto* pri = item.find("priority");
				t.priority = (pri && pri->is_num())
					? static_cast<int>(pri->as_num()) : 0;
				if (t.priority < 0 || t.priority > 2) t.priority = 0;
				fTodos.push_back(std::move(t));
			}

			// Build response summary.
			int pending = 0, in_progress = 0, completed = 0;
			for (const auto& t : fTodos) {
				if (t.status == "pending") ++pending;
				else if (t.status == "in_progress") ++in_progress;
				else if (t.status == "completed") ++completed;
			}

			std::string out;
			out += "Todo list updated (" + std::to_string(fTodos.size())
			       + " items: " + std::to_string(completed) + " completed, "
			       + std::to_string(in_progress) + " in progress, "
			       + std::to_string(pending) + " pending)\n\n";

			for (const auto& t : fTodos) {
				const char* mark = (t.status == "completed") ? "[x]"
					: (t.status == "in_progress") ? "[~]" : "[ ]";
				const char* pri_tag = (t.priority == 2) ? " !!!"
					: (t.priority == 1) ? " !" : "";
				out += std::string(mark) + " " + t.id + ": "
				       + t.content + pri_tag + "\n";
			}

			r.output = out;
		} catch (const std::exception& e) {
			r.is_error = true;
			r.output = std::string("parse error: ") + e.what();
		}
		return r;
	}

private:
	std::vector<TodoItem> fTodos;
};

}  // namespace

std::unique_ptr<Tool> MakeTodoWriteTool() {
	return std::make_unique<TodoWriteTool>();
}

}  // namespace pluto
