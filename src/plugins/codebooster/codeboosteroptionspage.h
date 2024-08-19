#ifndef CODEBOOSTEROPTIONSPAGE_H
#define CODEBOOSTEROPTIONSPAGE_H

#include <coreplugin/dialogs/ioptionspage.h>

namespace CodeBooster {

class CodeBoosterOptionsPage : public Core::IOptionsPage
{
public:
    CodeBoosterOptionsPage();

    static CodeBoosterOptionsPage &instance();
};


} // namespace CodeBooster

#endif // CODEBOOSTEROPTIONSPAGE_H
