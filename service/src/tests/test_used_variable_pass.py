
import lark

import compiler

TEST_CASES_AND_EXPECTED_OUTPUT = [
    ("""
    x = 10 + y;
    """, ['y'], ['x', 'y']),
    ("""
    if (x < y)
    {
      z = x + foo + bar - baz;
    }
    else
    {
      u = adam * baz;
      if (u == 10)
      {
        testing(x, y, another ^ foobar);
        defun youtube(x, adamd, asdf, unused)
        {
            adamd + asdf;
            while (x)
            {
              turn = it + up - 100;
            }
            return to_return - 1000;
        }
      }
    }
""",
     ['x', 'y', 'foo', 'bar', 'baz', 'adam', 'u', 'another', 'foobar', 'adamd', 'asdf', 'it', 'up', 'to_return'],
     ['x', 'y', 'foo', 'bar', 'baz', 'adam', 'u', 'another', 'foobar', 'z', 'adamd', 'asdf', 'it', 'up', 'turn', 'to_return']),
    ("""
defun strlen(s)
{
  i = 0;
  done = 0;
  while (done != 1)
  {
    shifted = s >> i;
	char = shifted & 0xff;
	if (char == 0)
	{
	  done = 1;
  	}
	else
	{
	  i = i + 1;
	}
  }
  return i;
}
""",
     ["s", "i", "done", "shifted", "char"],
     ["s", "i", "done", "shifted", "char"])
]

def test_used_variable_pass():
    with open(compiler.GRAMMAR_FILE, 'r') as grammar:
        parser = lark.Lark(grammar, start='program')

    for program, success_no_lhs, success_use_lhs in TEST_CASES_AND_EXPECTED_OUTPUT:
        tree = parser.parse(program)

        output_no_lhs = set(compiler.UsedVariablesPass(False).transform(tree))
        assert(output_no_lhs == set(success_no_lhs))

        output_with_lhs = set(compiler.UsedVariablesPass(True).transform(tree))
        assert(output_with_lhs == set(success_use_lhs))

    

