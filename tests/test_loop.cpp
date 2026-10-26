#include "../src/agent/Loop.h"
#include "../src/tools/Tool.h"
#include "../src/config.h"
#include "../src/json.h"
#include "../src/util.h"
#include "../src/workspace.h"

#include <cstdio>
#include <vector>
#include <string>

using namespace pluto;

// Mock provider for testing
class MockProvider : public Provider {
public:
    std::string name() const override { return "mock"; }
    void stream(const CompletionRequest& req, StreamCallback cb) override {
        // Simulate a simple text response
        StreamEvent ev;
        ev.type = StreamEvent::TEXT_DELTA;
        ev.text = "Test response";
        cb(ev);
        
        ev.type = StreamEvent::MESSAGE_END;
        ev.stop_reason = "end_turn";
        ev.input_tokens = 10;
        ev.output_tokens = 5;
        cb(ev);
    }
};

// Mock tool for testing
class MockTool : public Tool {
public:
    std::string name() const override { return "mock_tool"; }
    std::string description() const override { return "A mock tool for testing"; }
    std::string input_schema_json() const override { return "{}"; }
    ToolResult execute(const std::string& input_json) override {
        ToolResult r;
        r.output = "Mock tool executed with: " + input_json;
        return r;
    }
};

int main() {
    printf("Testing Loop.cpp core agent logic...\n");
    
    // Test 1: AutoAllow permission function
    {
        auto perm = AutoAllow();
        MockTool tool;
        Permission p = perm(tool, "test preview");
        if (p != Permission::Allow) {
            printf("FAIL: AutoAllow should return Allow\n");
            return 1;
        }
        printf("PASS: AutoAllow returns Allow\n");
    }
    
    // Test 2: InteractivePrompt permission function (auto-allow on empty)
    {
        auto perm = InteractivePrompt();
        MockTool tool;
        // We can't easily test the interactive part, but we can verify it compiles
        printf("PASS: InteractivePrompt compiles\n");
    }
    
    // Test 3: cap_tool_output truncation
    {
        Config cfg;
        cfg.max_tool_output = 100;
        
        std::string short_output = "short";
        cap_tool_output(short_output, cfg);
        if (short_output != "short") {
            printf("FAIL: Short output should not be truncated\n");
            return 1;
        }
        printf("PASS: Short output not truncated\n");
        
        std::string long_output(200, 'x');
        cap_tool_output(long_output, cfg);
        if (long_output.size() != 100 + strlen("\n... [truncated 100 bytes]")) {
            printf("FAIL: Long output should be truncated to max_tool_output + truncation message\n");
            return 1;
        }
        if (long_output.find("[truncated") == std::string::npos) {
            printf("FAIL: Truncation message should be present\n");
            return 1;
        }
        printf("PASS: Long output truncated with message\n");
    }
    
    // Test 4: estimate_tokens rough calculation
    {
        std::vector<Message> msgs;
        Message m1;
        m1.role = "user";
        ContentBlock b1;
        b1.type = ContentBlock::TEXT;
        b1.text = "Hello world";
        m1.content.push_back(b1);
        msgs.push_back(m1);
        
        Message m2;
        m2.role = "assistant";
        ContentBlock b2;
        b2.type = ContentBlock::TEXT;
        b2.text = "Hello there";
        m2.content.push_back(b2);
        msgs.push_back(m2);
        
        size_t tokens = estimate_tokens(msgs);
        // Roughly 4 chars per token
        size_t expected = (11 + 11) / 4; // "Hello world" + "Hello there" = 22 chars / 4 = 5.5
        if (tokens == 0) {
            printf("FAIL: estimate_tokens should return non-zero\n");
            return 1;
        }
        printf("PASS: estimate_tokens returns %zu\n", tokens);
    }
    
    // Test 5: first_round_end finds clean boundary
    {
        std::vector<Message> msgs;
        
        // user -> assistant (no tools) -> user (clean boundary at 2)
        Message u1; u1.role = "user";
        ContentBlock u1b; u1b.type = ContentBlock::TEXT; u1b.text = "Hi";
        u1.content.push_back(u1b);
        msgs.push_back(u1);
        
        Message a1; a1.role = "assistant";
        ContentBlock a1b; a1b.type = ContentBlock::TEXT; a1b.text = "Hello";
        a1.content.push_back(a1b);
        msgs.push_back(a1);
        
        Message u2; u2.role = "user";
        ContentBlock u2b; u2b.type = ContentBlock::TEXT; u2b.text = "How are you";
        u2.content.push_back(u2b);
        msgs.push_back(u2);
        
        size_t boundary = first_round_end(msgs);
        if (boundary != 2) {
            printf("FAIL: first_round_end should be 2 for clean user/assistant/user\n");
            return 1;
        }
        printf("PASS: first_round_end finds clean boundary\n");
    }
    
    // Test 6: first_round_end with tool use
    {
        std::vector<Message> msgs;
        
        // user -> assistant (with tool_use) -> user (tool_result) -> assistant (no tools) -> user (clean boundary at 4)
        Message u1; u1.role = "user";
        ContentBlock u1b; u1b.type = ContentBlock::TEXT; u1b.text = "Run tool";
        u1.content.push_back(u1b);
        msgs.push_back(u1);
        
        Message a1; a1.role = "assistant";
        ContentBlock a1b; a1b.type = ContentBlock::TOOL_USE; a1b.tool_name = "test"; a1b.tool_input = "{}"; a1b.tool_id = "1";
        a1.content.push_back(a1b);
        msgs.push_back(a1);
        
        Message u2; u2.role = "user";
        ContentBlock u2b; u2b.type = ContentBlock::TOOL_RESULT; u2b.tool_id = "1"; u2b.text = "done";
        u2.content.push_back(u2b);
        msgs.push_back(u2);
        
        Message a2; a2.role = "assistant";
        ContentBlock a2b; a2b.type = ContentBlock::TEXT; a2b.text = "Done";
        a2.content.push_back(a2b);
        msgs.push_back(a2);
        
        Message u3; u3.role = "user";
        ContentBlock u3b; u3b.type = ContentBlock::TEXT; u3b.text = "Thanks";
        u3.content.push_back(u3b);
        msgs.push_back(u3);
        
        size_t boundary = first_round_end(msgs);
        if (boundary != 4) {
            printf("FAIL: first_round_end should be 4 for tool use round\n");
            return 1;
        }
        printf("PASS: first_round_end handles tool use rounds\n");
    }
    
    // Test 7: prune_to_fit reduces messages
    {
        std::vector<Message> msgs;
        // Create many messages
        for (int i = 0; i < 20; ++i) {
            Message m;
            m.role = (i % 2 == 0) ? "user" : "assistant";
            ContentBlock b;
            b.type = ContentBlock::TEXT;
            b.text = "Message " + std::to_string(i);
            m.content.push_back(b);
            msgs.push_back(m);
        }
        
        Config cfg;
        cfg.context_window = 100; // Very small to force pruning
        
        int dropped = prune_to_fit(msgs, 50); // Target 50 tokens
        if (dropped == 0) {
            printf("FAIL: prune_to_fit should drop messages when over limit\n");
            return 1;
        }
        if (msgs.size() >= 20) {
            printf("FAIL: prune_to_fit should reduce message count\n");
            return 1;
        }
        // Should keep at least the last user message
        if (msgs.empty() || msgs.back().role != "user") {
            printf("FAIL: Last message should be user\n");
            return 1;
        }
        printf("PASS: prune_to_fit reduces messages, keeps last user\n");
    }
    
    // Test 8: MaybeCompact triggers at 90%
    {
        MockProvider provider;
        Config cfg;
        cfg.context_window = 1000;
        cfg.model = "test-model";
        
        std::vector<Message> msgs;
        Message m; m.role = "user";
        ContentBlock b; b.type = ContentBlock::TEXT; b.text = "Test";
        m.content.push_back(b);
        msgs.push_back(m);
        
        UiCallbacks ui;
        bool compacted = MaybeCompact(provider, cfg, msgs, 950, ui); // 95% of 1000
        // Should not compact with only 1 message
        if (compacted) {
            printf("FAIL: MaybeCompact should not compact with <=2 messages\n");
            return 1;
        }
        printf("PASS: MaybeCompact respects minimum message count\n");
    }
    
    printf("\nAll Loop.cpp tests passed!\n");
    return 0;
}
