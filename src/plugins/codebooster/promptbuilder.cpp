#include "promptbuilder.h"

#include "codeboostersettings.h"

namespace CodeBooster::Internal {
PromptBuilder::PromptBuilder() {}

QString PromptBuilder::getCompletionPrompt(const QString &prefix, const QString &suffix)
{
    QString task  = "TASK: Fill the {{FILL_HERE}} hole. Answer only with the CORRECT completion, and NOTHING ELSE. Do it now.";

    QString fullPrompt =
        "\n\n<QUERY>\n" +
        prefix +
        "{{FILL_HERE}}"+
        suffix +
        "\n</QUERY>\n" +
        task;

    return fullPrompt;
}

QString PromptBuilder::systemMessage()
{
    QString prompt;

    // todo: 使用用户自定义prompt
    bool useCustomPrompt = false;
    if (!useCustomPrompt)
    {
        prompt = R"(
You are a HOLE FILLER. You are provided with a file containing holes, formatted as '{{HOLE_NAME}}'. Your TASK is to complete with a string to replace this hole with, inside a <COMPLETION/> XML tag, including context-aware indentation, if needed.  All completions MUST be truthful, accurate, well-written and correct.

## EXAMPLE QUERY:

<QUERY>
function sum_evens(lim) {
  var sum = 0;
  for (var i = 0; i < lim; ++i) {
    {{FILL_HERE}}
  }
  return sum;
}
</QUERY>

TASK: Fill the {{FILL_HERE}} hole.

## CORRECT COMPLETION

<COMPLETION>if (i % 2 === 0) {
      sum += i;
    }</COMPLETION>

## EXAMPLE QUERY:

<QUERY>
def sum_list(lst):
  total = 0
  for x in lst:
  {{FILL_HERE}}
  return total

print sum_list([1, 2, 3])
</QUERY>

## CORRECT COMPLETION:

<COMPLETION>  total += x</COMPLETION>

## EXAMPLE QUERY:

<QUERY>
// data Tree a = Node (Tree a) (Tree a) | Leaf a

// sum :: Tree Int -> Int
// sum (Node lft rgt) = sum lft + sum rgt
// sum (Leaf val)     = val

// convert to TypeScript:
{{FILL_HERE}}
</QUERY>

## CORRECT COMPLETION:

<COMPLETION>type Tree<T>
  = {$:"Node", lft: Tree<T>, rgt: Tree<T>}
  | {$:"Leaf", val: T};

function sum(tree: Tree<number>): number {
  switch (tree.$) {
    case "Node":
      return sum(tree.lft) + sum(tree.rgt);
    case "Leaf":
      return tree.val;
  }
}</COMPLETION>

## EXAMPLE QUERY:

The 4th {{FILL_HERE}} is Jupiter.

## CORRECT COMPLETION:

<COMPLETION>the 4th planet after Mars</COMPLETION>

## EXAMPLE QUERY:

function hypothenuse(a, b) {
  return Math.sqrt({{FILL_HERE}}b ** 2);
}

## CORRECT COMPLETION:

<COMPLETION>a ** 2 + </COMPLETION>;
)";
    }

    return prompt;
}

QStringList PromptBuilder::stopCodes()
{
    QStringList codes;
    codes << "<COMPLETION>"
          << "</COMPLETION>";

    return codes;
}

}
