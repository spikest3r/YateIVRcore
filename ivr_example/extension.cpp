#include <unordered_map>
#include <string>
#include <functional>
#include <variant>

struct NativeDescriptor {
    std::string name;
    uint8_t opcode;
    uint8_t argCount;
};

struct CallInfo {
    std::string call_id;
    char lastDtmfCommand;
};

struct CallFrame {
    int returnPC;
    int routineBase;
};

typedef enum {
    TAG_INT = 2,
    TAG_FLOAT = 3,
    TAG_STRING = 1
} TypeTag;

typedef struct {
    TypeTag type;
    std::variant<int64_t, double, std::string> data;
} Variant;

struct VMExecutionData {
    std::vector<Variant> variables;
    std::vector<Variant> stack;
    std::vector<CallFrame> pcStack;

    int PC = 0;
    int routineBase = 0;
    bool halt = false;

    CallInfo callInfo;
};

using NativeFn = std::function<void(VMExecutionData*)>;

extern "C" __declspec(dllexport)
void RegisterNatives(std::unordered_map<int, NativeFn>& funcMap,
    std::vector<NativeDescriptor>& descriptors) {

    // to avoid collisions with core functions, use E0-FF space
    // compiler bug requires specifing 1 arg required even when you dont need arguments

    funcMap[0xE0] = [](VMExecutionData* execData) {
        // your code here
    };
    descriptors.push_back({ "someFunction", 0xE0, 1 });
}