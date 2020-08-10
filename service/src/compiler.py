#!/usr/bin/env python3
import argparse
import collections
import dataclasses
import itertools
import logging
import os
import random
import sys
import tempfile
import typing

import lark

import assembler

l = logging.getLogger("compiler")

GRAMMAR_FILE = os.path.dirname(os.path.realpath(__file__)) + '/grammar.lark'

OP_TO_ASM = {
    'add': 'ADD',
    'sub': 'SUB',
    'mul': 'MUL',
    'div': 'NOT_IMPLEMENTED',
    'bit_and': 'AND',
    'bit_xor': 'XOR',
    'bit_or': 'OR',
    'bit_shift_left': 'SHL',
    'bit_shift_right': 'SHR',
    'equals': 'EQ',
    'not_equals': 'NEQ',
    'less_than' : 'LT',
    'greater_than' : 'GT',
    'less_than_or_equal': 'LTE',
    'greater_than_or_equal': 'GTE',
}

@dataclasses.dataclass
class Function:
    name: str
    scope: str
    arg_list: typing.List[str]
    is_special: bool = False
    has_return_value: bool = True
    is_external: bool = False

    def arg_name(self, idx):
        name = f"_{self.name}_arg_{idx}"
        if self.is_external:
            name += "_export"
        return name

    def return_location(self):
        name = f"_{self.name}_return_location"
        if self.is_external:
            name += "_export"
        return name

SPECIAL_FUNCTIONS = [Function("OUTD", "", ["output"], True, False),
                     Function("OUTS", "", ["output"], True, False),
                     Function("OPEN", "", ["filename", "flags"], True, True),
                     Function("READ", "", ["fd"], True, True),
                     Function("WRITE", "", ["fd", "byte"], True, True),
                     Function("CLOSE", "", ["fd"], True, True),
                     Function("HALT", "", ["status"], True, True),
                     Function("LOAD", "", ["filename", "arg"], True, True),
                     Function("LISTDIR", "", ["dir"], True, True),
                     Function("SENDFILE", "", ["in", "out"], True, True),
                     Function("UNLINK", "", ["path"], True, True),
                     Function("LSEEK", "", ["fd", "offset"], True, True),
                     Function("RANDOM", "", ["input"], True, True),
]

GENERATE_ASSEMBLY_SPECIAL_FUNCTIONS = {'OUTD': lambda args: f"OUTD {args[0]}\n",
                                       'OUTS': lambda args: f"OUTS {args[0]}\n",
                                       'OPEN': lambda args: f"{args[-1]} = OPN {args[0]} {args[1]}\n",
                                       'READ': lambda args: f"{args[-1]} = RED {args[0]}\n",
                                       'WRITE': lambda args: f"{args[-1]} = WRT {args[0]} {args[1]}\n",
                                       'CLOSE': lambda args: f"{args[-1]} = CLS {args[0]}\n",
                                       'HALT': lambda args: f"{args[-1]} = HLT {args[0]}\n",
                                       'LOAD': lambda args: f"{args[-1]} = LOD {args[0]} {args[1]}\n",
                                       'LISTDIR': lambda args: f"{args[-1]} = LS {args[0]}\n",
                                       'SENDFILE': lambda args: f"{args[-1]} = SDF {args[0]} {args[1]}\n",
                                       'UNLINK': lambda args: f"{args[-1]} = ULK {args[0]}\n",
                                       'LSEEK': lambda args: f"{args[-1]} = LSK {args[0]} {args[1]}\n",
                                       'RANDOM': lambda args: f"{args[-1]} = RND {args[0]}\n"
}

class ExtractFunctionsPass(lark.visitors.Interpreter):
    functions = list()
    scope = list()
    
    def function_def(self, tree):
        function_name = str(tree.children[0].children[0])
        args = [str(arg.children[0]) for arg in tree.children[1].children]
        scope = ":".join(self.scope)

        self.functions.append(Function(function_name, scope, args))

        self.scope.append(function_name)
        self.visit_children(tree)
        self.scope.pop()

    def extern_statement(self, tree):
        function_name = str(tree.children[0].children[0])
        args = [str(arg.children[0]) for arg in tree.children[1].children]
        scope = ":".join(self.scope)

        self.functions.append(Function(function_name, scope, args, is_external=True))

class UsedVariablesPass(lark.Transformer):

    def __init__(self, count_lhs = False):
        self.count_lhs = count_lhs

    def _flatten(self, tree):
        to_return = list(itertools.chain.from_iterable(tree))
        done = False
        while not done:
            if all(not isinstance(element, list) for element in to_return):
                done = True
            else:
                to_return = list(itertools.chain.from_iterable(to_return))
        return to_return


    statements = _flatten
    program = _flatten
    function_def = _flatten
    conditional = _flatten
    while_loop = _flatten
    elif_clauses = _flatten
    function_call = _flatten
    else_clause = _flatten
    expression_function_arg_list = _flatten
    return_statement = _flatten

    arg_list = lambda self, _: list()
    function_name = lambda self, _: list()
    number = lambda self, _: list()
    hexnumber = lambda self, _: list()
    string  = lambda self, _: list()
    outs_literal = lambda self, _: list()
    extern_statement = lambda self, _: list()
    asm_statement = lambda self, _: list()

    def lhs(self, tree):
        if self.count_lhs:
            return tree[0]
        else:
            return []

    def binary_operation(self, tree):
        tree[2].extend(tree[0])
        return tree[2]

    def assignment(self, tree):
        tree[0].extend(tree[1])
        return tree[0]

    def var_name(self, tree):
        return [str(tree[0])]

class GenerateAssemblyPass(lark.visitors.Interpreter):
    functions: typing.Dict[str, Function]
    to_return: str = ""
    variables = dict()
    return_variables = set()
    current_scope_tag = None
    first_call_to_current_scope_tag = None
    current_conditional_true_var = None
    
    def __init__(self, functions):
        self.functions = {f.name: f for f in functions}
        self._num = 0

    def _new_temp_variable(self):
        self._num += 1
        return f"_tmp_{self._num}"

    def conditional(self, tree):
        condition_var = self.visit(tree.children[0])

        used_variables = set()
        for statements in tree.find_data('statements'):
            used_vars_in_this = UsedVariablesPass(True).transform(statements)
            used_variables.update(used_vars_in_this)

        # Filter out all the undefined vars
        used_variables = set(v for v in used_variables if v in self.variables)
        # create all the true and false versions of the used variables

        past_variables = self.variables.copy()

        branch_variables = dict()
        for var in used_variables:
            current_var = self.variables[var]
            true_var = f"_{var}{self._new_temp_variable()}_true"
            false_var = f"_{var}{self._new_temp_variable()}_false"
            branch_variables[var] = (true_var, false_var)

            self.variables[var] = true_var
            self.to_return += f"{true_var}, {false_var} = BRR {current_var} {condition_var}\n"

        # Set up a true conditional variable that constants will use
        # to guard their propagation
        prior_conditional_true_var = self.current_conditional_true_var

        true_value = self._create_num_literal(1, f"{self._new_temp_variable()}_1")
        self.current_conditional_true_var = f"{self._new_temp_variable()}_true"
        conditional_false_var = f"{self._new_temp_variable()}_false"
        self.to_return += f"{self.current_conditional_true_var}, {conditional_false_var} = BRR {true_value} {condition_var}\n"

        self.visit(tree.children[1])

        # Go through the variables, and add those that have changed to
        # the ones that need to be merged.

        after_true_variables = self.variables.copy()

        self.current_conditional_true_var = conditional_false_var

        self.variables = past_variables.copy()
        # Set up all the variables for the false branch
        for var in used_variables:
            false_var = branch_variables[var][1]
            self.variables[var] = false_var

        if tree.children[-1].data == 'else_clause':
            self.visit(tree.children[-1])

        after_false_variables = self.variables.copy()
        self.current_conditional_true_var = prior_conditional_true_var

        # Ok, seems like we're done, let's do all the merges
        new_variables = set(after_false_variables.keys())
        new_variables.update(after_true_variables.keys())

        for var in new_variables:
            if var in after_true_variables and var in after_false_variables:
                true_name = after_true_variables[var]
                false_name = after_false_variables[var]
                if true_name == false_name:
                    continue
                new_name = f"_{var}_phi{self._new_temp_variable()}"
                self.to_return += f"{new_name} = MER {true_name} {false_name}\n"
                self.variables[var] = new_name

            elif var in after_true_variables:
                self.variables[var] = after_true_variables[var]
            elif var in after_false_variables:
                self.variables[var] = after_false_variables[var]
            else:
                assert(False) # Should be impossible to reach here

    def while_loop(self, tree):
        prior_variables = self.variables.copy()

        undefined_before_loop = set()
        
        used_variables = set(UsedVariablesPass(True).transform(tree.children[0]))
        used_variables.update(UsedVariablesPass(True).transform(tree.children[1]))
        for var in used_variables:
            if not var in prior_variables:
                prior_variables[var] = f"_{var}_not_defined_before_loop{self._new_temp_variable()}"
                undefined_before_loop.add(var)

        loop_variables = dict()

        new_tag_with_old_tag = f"{self._new_temp_variable()}_new_tag_with_old"
        old_tag_with_old_tag = f"{self._new_temp_variable()}_old_tag_with_old"
        old_tag_with_new_tag = f"{self._new_temp_variable()}_old_tag_with_new"

        a_used_variable = None

        for var in used_variables:
            prior_name = prior_variables[var]

            if not a_used_variable and not var in undefined_before_loop:
                self.to_return += f"{new_tag_with_old_tag}, {old_tag_with_old_tag} = NTG {prior_name}\n"
                self.to_return += f"{old_tag_with_new_tag} = CTG {new_tag_with_old_tag} {old_tag_with_old_tag}\n"
                a_used_variable = var

            tagged_prior_variable = f"_{var}_loop_input_tagged{self._new_temp_variable()}"

            last_value_name = f"_{var}_loop_last_value{self._new_temp_variable()}"
            again_name = f"_{var}_loop_again{self._new_temp_variable()}"
            start_name = f"_{var}_loop_start{self._new_temp_variable()}"
            end_name = f"_{var}_loop_end{self._new_temp_variable()}"
            loop_variables[var] = (prior_name, start_name, last_value_name, again_name, end_name)

            self.to_return += f"{tagged_prior_variable} = CTG {new_tag_with_old_tag} {prior_name}\n"
            self.to_return += f"{start_name} = MER {tagged_prior_variable} {last_value_name}\n"
            self.variables[var] = start_name

        prior_current_conditional_true_var = self.current_conditional_true_var
        prior_scope_tag = self.current_scope_tag
        prior_first_call = self.first_call_to_current_scope_tag
        

        a_variable_start_name = loop_variables[a_used_variable][1]
        current_loop_tag = f"_loop_current_tag{self._new_temp_variable()}"
        self.to_return += f"{current_loop_tag} = ETG {a_variable_start_name}\n"
        
        self.current_scope_tag = current_loop_tag
        self.first_call_to_current_scope_tag = None
        self.current_conditional_true_var = None
        
        # check the condition
        condition_var = self.visit(tree.children[0])

        a_variable_again_name = loop_variables[a_used_variable][3]
        next_loop_tag = f"_loop_next_tag{self._new_temp_variable()}"
        self.to_return += f"{next_loop_tag} = ETG {a_variable_again_name}\n"

        self.current_scope_tag = next_loop_tag
        self.first_call_to_current_scope_tag = None
        self.current_conditional_true_var = None

        # create the output BRRs
        for var in used_variables:
            prior_name, start_name, last_value_name, again_name, end_name = loop_variables[var]
            tmp_true_name = f"_{var}_true{self._new_temp_variable()}"
            tmp_false_name = f"_{var}_false{self._new_temp_variable()}"
            
            self.to_return += f"{tmp_true_name}, {tmp_false_name} = BRR {start_name} {condition_var}\n"
            self.to_return += f"{again_name} = ITG {tmp_true_name}\n"

            reset_iteration_level_output = f"_{var}_reset_il{self._new_temp_variable()}"
            self.to_return += f"{reset_iteration_level_output} = SIL {tmp_false_name} 0\n"

            # reset the tags
            self.to_return += f"{end_name} = CTG {old_tag_with_new_tag} {reset_iteration_level_output}\n"

            self.variables[var] = again_name

        self.visit(tree.children[1])

        # connect the variables that are at the end of the loop body
        # to the start of the loop
        for var in used_variables:
            end_of_body_name = self.variables[var]
            prior_name, start_name, last_value_name, again_name, end_name = loop_variables[var]
            self.to_return += f"{last_value_name} = DUP {end_of_body_name}\n"
            self.variables[var] = end_name

        self.current_scope_tag = prior_scope_tag
        self.first_call_to_current_scope_tag = prior_first_call
        self.current_conditional_true_var = prior_current_conditional_true_var
        

    def return_statement(self, tree):
        return_expression = self.visit(tree.children[0])
        self.return_variables.add(return_expression)

    def function_def(self, tree):
        function_name = str(tree.children[0].children[0])
        args = [str(arg.children[0]) for arg in tree.children[1].children]
        function = self.functions[function_name]

        self.to_return += f"# Defining function {function_name}\n"

        # All constants in the function need to be tagged with the
        # proper tag (which will be the same as arg_0), so we extract
        # that tag and propagate it to any constants in the body of
        # the function. Need to set the call (the ETG) to the internal arg name
        prior_scope_tag = self.current_scope_tag
        prior_first_call = self.first_call_to_current_scope_tag
        self.current_scope_tag = f"{self._new_temp_variable()}_function_tag"

        prior_vars = self.variables.copy()

        i = 0
        for arg in args:
            # We want only one external argument point (so that
            # incoming calls only need to pass arguments to one
            # place), so we make an internal copy of the argument.
            external_arg_name = function.arg_name(i)
            internal_arg_name = f"{self._new_temp_variable()}_internal_{external_arg_name}"
            self.to_return += f"{external_arg_name}_export:\n"
            self.to_return += f"{internal_arg_name} = DUP {external_arg_name}\n"
            self.variables[arg] = internal_arg_name

            if i == 0:
                self.first_call_to_current_scope_tag = f"{self.current_scope_tag} = ETG {internal_arg_name}\n"

            i += 1

        # this is how we'll track each return statement, and merge them all into one
        prior_return_variables = self.return_variables.copy()

        # now visit inside the function
        self.visit_children(tree.children[2])

        if len(self.return_variables) == 0:
            l.error(f"Function {function_name} does not have any return statements.")
            sys.exit(-1)

        # Create the proper output block for the return variables.
        # Merge all the possible returns.
        prior_return = self.return_variables.pop()

        for return_var in self.return_variables:
            new_return = self._new_temp_variable()
            self.to_return += f"{new_return} = MER {prior_return} {return_var}\n"
            prior_return = new_return

        self.to_return += f"{function.return_location()}_export:\n"
        exported_return_loc = self._new_temp_variable()
        self.to_return += f"{exported_return_loc} = DUP {function.return_location()}\n"
        self.to_return += f"_ = RTD {prior_return} {exported_return_loc}\n"

        # now these variables go out of scope
        self.variables = prior_vars
        self.return_variables = prior_return_variables
        self.current_scope_tag = prior_scope_tag
        self.first_call_to_current_scope_tag = prior_first_call

    def var_name(self, tree):
        var_name = str(tree.children[0])

        if not var_name in self.variables:
            l.error(f"variable {var_name} not defined.")
            sys.exit(-1)

        return self.variables[var_name]

    def outs_literal(self, tree):
        string = eval(str(tree.children[0]))
        for i in range(0, len(string), 8):
            current = string[i:i+8]
            to_parse = repr(current)

            function_name = lark.Tree('function_name', [lark.Token('CNAME', 'OUTS')])
            args = lark.Tree('expression_function_arg_list', [lark.Tree('string', [lark.Token('STRING', to_parse)])])

            function_call = lark.Tree('function_call', [function_name, args])

            self.visit(function_call)

    def function_call(self, tree):
        function_name = str(tree.children[0].children[0])
        function_args = [self.visit(c) for c in tree.children[1].children] if len(tree.children) == 2 else []

        if function_name in self.functions:
            function = self.functions[function_name]
            if len(function_args) != len(function.arg_list):
                l.error(f"Call to function {function_name} has the wrong number of arguments {len(function_args)}, expected {len(function.arg_list)}")
                sys.exit(-1)

            if function.is_special:
                l.debug(f"Special function call {function_name}")
                if function.has_return_value:
                    return_var = f"_{function_name}_return_{self._new_temp_variable()}"
                    to_send = list(function_args)
                    to_send.append(return_var)
                    self.to_return += GENERATE_ASSEMBLY_SPECIAL_FUNCTIONS[function_name](to_send)
                    return return_var
                else:
                    self.to_return += GENERATE_ASSEMBLY_SPECIAL_FUNCTIONS[function_name](function_args)
                    return None
            else:
                l.debug(f"Normal fuction call {function_name}")

                self.to_return += f"# Function call to {function_name}\n"

                # As of now, don't have a mechanism to call a no-argument function
                # Could be done, let's punt until we actually need it
                assert(len(function_args) > 0)

                # take the first argument, pass it through NTG to get
                # a new tag, pass that tag to all other arguments, and
                # set it up properly

                new_tag_with_old_tag = f"{self._new_temp_variable()}_new_tag_with_old"
                old_tag_with_old_tag = f"{self._new_temp_variable()}_old_tag_with_old"
                new_tag_with_new_tag = f"{self._new_temp_variable()}_new_tag_with_new"
                old_tag_with_new_tag = f"{self._new_temp_variable()}_old_tag_with_new"

                arg_0 = function_args[0]

                self.to_return += f"{new_tag_with_old_tag}, {old_tag_with_old_tag} = NTG {arg_0}\n"
                self.to_return += f"{new_tag_with_new_tag} = CTG {new_tag_with_old_tag} {new_tag_with_old_tag}\n"
                self.to_return += f"{old_tag_with_new_tag} = CTG {new_tag_with_old_tag} {old_tag_with_old_tag}\n"

                i = 0
                for arg in function_args:
                    function_arg_name = function.arg_name(i)
                    i += 1
                    self.to_return += f"{function_arg_name} = CTG {new_tag_with_old_tag} {arg}\n"


                return_label = f"{self._new_temp_variable()}_{function_name}_return_label"
                return_location = function.return_location()
                self.to_return += f"{return_location} = CTG {new_tag_with_new_tag} {return_label}\n"

                returned_tagged_var = f"{self._new_temp_variable()}_return_value_tagged"

                self.to_return += f"{return_label}:\n{returned_tagged_var} = DUP _\n"

                returned_normal_tagged_var = f"{self._new_temp_variable()}_return_value"
                self.to_return += f"{returned_normal_tagged_var} = CTG {old_tag_with_new_tag} {returned_tagged_var}\n"

                return returned_normal_tagged_var

        else:
            l.error(f"Call to undefined function {function_name}.")
            sys.exit(-1)

    def assignment(self, tree):
        expression_result = self.visit(tree.children[1])
        variable_name = str(tree.children[0].children[0].children[0])
        new_variable_name = f"{variable_name}{self._new_temp_variable()}"

        self.to_return += f"{new_variable_name} = DUP {expression_result}\n"
        self.variables[variable_name] = new_variable_name
        return None

    def binary_operation(self, tree):
        op = tree.children[1].data

        assembly_op = OP_TO_ASM[op]

        left_var = self.visit(tree.children[0])
        right_var = self.visit(tree.children[2])
        
        new_variable = self._new_temp_variable()
        self.to_return += f"{new_variable} = {assembly_op} {left_var} {right_var}\n"

        return new_variable

    def number(self, tree):
        # For now we do this very inefficient thing where we DUP each
        # literal string (so that the binary operator doesn't have to
        # worry about which literal can go where)
        
        new_variable = self._new_temp_variable()
        num = tree.children[0]
        self._create_num_literal(num, new_variable)
        return new_variable


    def _create_num_literal(self, num, new_variable):
        if self.current_conditional_true_var:
            old_new_variable = new_variable
            new_variable = self._new_temp_variable()

        if self.current_scope_tag:
            if self.first_call_to_current_scope_tag:
                self.to_return += self.first_call_to_current_scope_tag
                self.first_call_to_current_scope_tag = None

            self.to_return += f"{new_variable} = CTG {self.current_scope_tag} {num}\n"
        else:
            self.to_return += f"{new_variable} = DUP {num}\n"

        if self.current_conditional_true_var:
            self.to_return += f"{old_new_variable}, _ = BRR {new_variable} {self.current_conditional_true_var}\n"
            new_variable = old_new_variable

        return new_variable

    def hexnumber(self, tree):
        num = int(tree.children[0], 16)
        new_variable = self._new_temp_variable()
        self._create_num_literal(num, new_variable)
        return new_variable

    def string(self, tree):
        the_str = eval(str(tree.children[0]))
        if len(the_str) > 8:
            l.error(f"string literal {the_str} is larger than the limit of 8.")
            sys.exit(-1)

        the_str_binary = the_str.encode()

        int_repr = int.from_bytes(the_str_binary, 'little')
        new_variable = self._new_temp_variable()
        self._create_num_literal(int_repr, new_variable)
        return new_variable

    def export_statement(self, tree):
        function_name = str(tree.children[0].children[0])
        if not function_name in self.functions:
            l.error(f"Attempt to export function {function_name} but it's not defined.")
            sys.exit(-1)

        function = self.functions[function_name]
        i = 0
        for arg in function.arg_list:
            self.to_return += f"export {function.arg_name(i)}_export\n"
            i += 1

        self.to_return += f"export {function.return_location()}_export\n"

    def extern_statement(self, tree):
        function_name = str(tree.children[0].children[0])
        if not function_name in self.functions:
            # Should never happen, the other pass should pick this up
            assert(False)

        function = self.functions[function_name]
        i = 0
        for arg in function.arg_list:
            self.to_return += f"extern {function.arg_name(i)}\n"
            i += 1

        self.to_return += f"extern {function.return_location()}\n"

    def asm_statement(self, tree):
        the_str = eval(str(tree.children[0]))
        the_str = the_str.strip()

        self.to_return += f"{the_str}\n"


def main(input_file, output_file, assembly_output, graph_output, backdoor):

    with open(GRAMMAR_FILE, 'r') as grammar:
        parser = lark.Lark(grammar, start='program')
    
    with open(input_file, 'r') as input:
        tree = parser.parse(input.read())

    l.debug(f"parsed tree: {tree.pretty()}")

    extract_functions = ExtractFunctionsPass()
    extract_functions.visit(tree)

    l.debug(f"found functions={extract_functions.functions}")
    defined_functions = extract_functions.functions

    # "functions" that are actually instructions (which are actually MMIO)
    defined_functions.extend(SPECIAL_FUNCTIONS)

    generate_assembly = GenerateAssemblyPass(defined_functions)
    generate_assembly.visit(tree)

    assembly_code = generate_assembly.to_return

    if backdoor:
        with open(backdoor, 'r') as backdoor_file:
            assembly_code_lines = assembly_code.splitlines()
            content = backdoor_file.readlines()
            for c in content:
                c = c.strip()
                idx = random.randint(0, len(assembly_code_lines)-1)
                l.debug(f"backdoor: inserting line={c} into index={idx}")
                assembly_code_lines.insert(idx, c)
            assembly_code = "\n".join(assembly_code_lines)

    if assembly_output:
        with open(assembly_output, 'w') as saved_assembly:
            saved_assembly.write(assembly_code)

    with tempfile.NamedTemporaryFile('w+') as temp_assembly:
        temp_assembly.write(assembly_code)             
        temp_assembly.seek(0)
        assembler.main(temp_assembly.name, output_file, graph_output)

if __name__ == '__main__':
    parser = argparse.ArgumentParser(prog="compiler")
    parser.add_argument("--debug", action="store_true", help="Enable debugging")
    parser.add_argument("--file", type=str, required=True, help="The file to compile")
    parser.add_argument("--assembly", type=str, help="Where to write and store the assembly output.")
    parser.add_argument("--output", type=str, help="Where to write the binary output.")
    parser.add_argument("--graph", type=str, help="Where to write the graph dot output.")
    parser.add_argument("--backdoor", type=str, help="Assembly file to add as backdoor.")

    args = parser.parse_args()

    if args.debug:
        logging.basicConfig(level=logging.DEBUG)

    main(args.file, args.output or "output.bin", args.assembly, args.graph, args.backdoor)
    
