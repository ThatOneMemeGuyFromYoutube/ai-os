"""Lua-adjacent TinyLang parser and lowering pass.

This first source-language slice supports local scalar variables, integer
literals, self-updating + / - expressions, assignments, -- comments, and
return. It lowers to the private flat-i386 backend; users do not write x86.
"""
from __future__ import annotations
import re

NUMBER = re.compile(r"^(?:[0-9]+|0[xX][0-9A-Fa-f]+)$")
LOCAL = re.compile(r"^local\s+([A-Za-z_][A-Za-z0-9_]*)\s*=\s*(.+)$")
ASSIGN = re.compile(r"^([A-Za-z_][A-Za-z0-9_]*)\s*=\s*(.+)$")
UPDATE = re.compile(r"^([A-Za-z_][A-Za-z0-9_]*)\s*([+-])\s*(.+)$")
REGISTERS = ("eax", "ecx", "edx", "ebx", "esi", "edi")

def is_lua_source(source: str) -> bool:
    for raw in source.splitlines():
        line = raw.strip()
        if not line or line.startswith("--"):
            continue
        return line.startswith("local ") or line == "return" or bool(ASSIGN.match(line))
    return False

def _number(value: str, line: int) -> str:
    value = value.strip()
    if not NUMBER.fullmatch(value):
        raise ValueError(f"line {line}: expected an integer literal, got {value!r}")
    hexadecimal = value.lower().startswith("0x")
    digits = (value[2:] if hexadecimal else value).lstrip("0") or "0"
    if len(digits) > (8 if hexadecimal else 10):
        raise ValueError(f"line {line}: integer literal exceeds 32 bits")
    number = int(digits, 16 if hexadecimal else 10)
    if number > 0xFFFFFFFF:
        raise ValueError(f"line {line}: integer literal exceeds 32 bits")
    return str(number)

def lower_lua_source(source: str) -> str:
    variables: dict[str, str] = {}
    output: list[str] = []
    returned = False
    for line_no, raw in enumerate(source.splitlines(), 1):
        line = raw.split("--", 1)[0].strip()
        if not line:
            continue
        if line.endswith(";"):
            line = line[:-1].rstrip()
        if returned:
            raise ValueError(f"line {line_no}: no statements may follow return")
        if line == "return":
            output.append("exit")
            returned = True
            continue
        declaration = LOCAL.fullmatch(line)
        if declaration:
            name, expression = declaration.groups()
            if name in variables:
                raise ValueError(f"line {line_no}: duplicate local {name!r}")
            if len(variables) >= len(REGISTERS):
                raise ValueError(f"line {line_no}: TinyLang supports at most six local variables")
            register = REGISTERS[len(variables)]
            variables[name] = register
            output.append(f"mov {register}, {_number(expression, line_no)}")
            continue
        assignment = ASSIGN.fullmatch(line)
        if assignment:
            name, expression = assignment.groups()
            if name not in variables:
                raise ValueError(f"line {line_no}: unknown variable {name!r}; declare it with local")
            register = variables[name]
            update = UPDATE.fullmatch(expression)
            if update:
                target, operator, amount = update.groups()
                if target != name:
                    raise ValueError(f"line {line_no}: expressions must update their own variable")
                opcode = "add" if operator == "+" else "sub"
                output.append(f"{opcode} {register}, {_number(amount, line_no)}")
            else:
                output.append(f"mov {register}, {_number(expression, line_no)}")
            continue
        raise ValueError(f"line {line_no}: unsupported TinyLang statement; expected local, assignment, or return")
    if not output:
        raise ValueError("line 1: TinyLang program must contain at least one statement")
    if not returned:
        output.append("exit")
    return "\n".join(output) + "\n"
