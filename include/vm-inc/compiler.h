#pragma once
#include "includes.h"
#include "helpers.h"
#include "types.h"
#include "tokenizer.h"

int compile(std::string fileName,
    CompilerData* compilerData,
    std::unordered_map<std::string, uint32_t>& routineMap
);

void compileExpression(
    std::string expr, CompilerData* data, std::vector<uint8_t>& bytecode
);

struct Function {
    uint8_t opcode;
    uint8_t argCount;
};

extern std::unordered_map<std::string, Function> funcList;