#pragma once

#include "Framework.h"

namespace fw
{
template<typename T>
int runApplication()
{
    fw::Framework fw;
    int status = 1;
    if (fw.initialize())
    {
        T app;
        if (app.initialize())
        {
            fw.setApplication(&app);
            fw.execute();
            status = 0;
        }
    }

    return status;
}

} // namespace fw
