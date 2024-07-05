#ifndef CODEGEEX2OPTIONSPAGE_H
#define CODEGEEX2OPTIONSPAGE_H

#include <coreplugin/dialogs/ioptionspage.h>

namespace CodeBooster {

class CodeBoosterOptionsPage : public Core::IOptionsPage
{
public:
    CodeBoosterOptionsPage();

    static CodeBoosterOptionsPage &instance();
};
} // namespace CodeBooster

#endif // CODEGEEX2OPTIONSPAGE_H
