// ----------------------------------------------------------------
// Copyright (C) 2019 Alex Beharrell
//
// This file is part of penguinTrace.
//
// penguinTrace is free software: you can redistribute it and/or
// modify it under the terms of the GNU Affero General Public
// License as published by the Free Software Foundation, either
// version 3 of the License, or any later version.
//
// penguinTrace is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public
// License along with penguinTrace. If not, see
// <https://www.gnu.org/licenses/>.
// ----------------------------------------------------------------
//
// Code Examples

var codeExamples = new Object();

codeExamples['c-hello-world'] = {
  "name": "Hello World",
  "description": "Simple 'Hello World' example in C",
  "lang": ptrace.languages.c,
  "code": "#include <stdio.h>\n\nint main(int argc, char** argv)\n" +
          "{\n  printf(\"Hello, world!\\n\");\n  return 0;\n}"
};
codeExamples['c++-hello-world'] = {
  "name": "Hello World",
  "description": "Simple 'Hello World' example in C++",
  "lang": ptrace.languages.cxx,
  "code": "#include <iostream>\n\nint main(int argc, char** argv)\n" +
          "{\n  std::cout << \"Hello, world!\" << std::endl;\n  return 0;\n}"
};
codeExamples['c-factorial'] = {
  "name": "Factorial",
  "description": "Recursive function call example",
  "lang": ptrace.languages.c,
  "code": "#include <stdio.h>\n\nint factorial (int n)\n{\n" +
          "  if (n >= 1) return n*factorial(n-1);\n  else return 1;\n}\n\n" +
          "int main(int argc, char** argv)\n" +
          "{\n  for (int i = 0; i < 5; i++)\n  {\n" +
          "    printf(\"%d! = %d\\n\", i, factorial(i));\n" +
          "  }\n  return 0;\n}"
};
codeExamples['c-testing'] = {
  "name": "Factorial+",
  "description": "Factorial, printing and local variables",
  "lang": ptrace.languages.c,
  "code": "#include <stdio.h>\n\nint factorial (int n)\n{\n" +
          "  if (n >= 1) return n*factorial(n-1);\n  else return 1;\n}\n\n" +
          "int main(int argc, char** argv)\n" +
          "{\n" +
          "  int x = 0;\n" +
          "  int y = 5;\n" +
          "  int z = 7;\n" +
          "  int fact;\n" +
          "  x -= 1;\n" +
          "\n  for (int i = 0; i < 5; i++)\n  {\n" +
          "    fact = factorial(i);\n" +
          "  }\n" +
          "  printf(\"fact = %d\\n\", fact);\n" +
          "  printf(\"%d - %d - %d\\n\", x, y, z);\n" +
          "  printf(\"Hello, world!\\n\");\n" +
          "  return 0;\n}"
};
codeExamples['c-args'] = {
  "name": "Command Line Args",
  "description": "Printing of command line arguments",
  "lang": ptrace.languages.c,
  "code": "#include <stdio.h>\n\n" +
          "int main(int argc, char** argv)\n" +
          "{\n" +
          "\n  for (int i = 0; i < argc; i++)\n  {\n" +
          "    printf(\"%s\\n\", argv[i]);\n" +
          "  }\n" +
          "  return 0;\n}"
};

codeExamples['asm-aarch64-hello-world'] = {
  "name": "Hello World",
  "description": "Simple 'Hello World' example in AArch64 assembly",
  "lang": ptrace.languages.asm,
  "arch": ptrace.architectures.aarch64,
  "code": "  .globl _start\n_start:\n  mov x0, #1\n" +
          "  adr x1, hello\n  mov x2, #14\n" +
          "  mov x8, #64\n  svc #0\n  mov x0, #0\n" +
          "  mov x8, #93\n  svc #0\n\n" +
          "  .data\nhello:\n" +
          "  .string \"Hello, world!\\n\""
};

codeExamples['asm-x86_64-hello-world'] = {
  "name": "Hello World",
  "description": "Simple 'Hello World' example in x86_64 assembly",
  "lang": ptrace.languages.asm,
  "arch": ptrace.architectures.x86_64,
  "code": "  .globl _start\n_start:\n  mov $1, %rdi\n" +
          "  mov $hello, %rsi\n  mov $14, %rdx\n" +
          "  mov $1, %rax\n  syscall\n  xor %rdi, %rdi\n" +
          "  mov $60, %rax\n  syscall\n\n" +
          "  .data\nhello:\n" +
          "  .string \"Hello, world!\\n\""
};
