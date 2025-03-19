#include "GapBuffer.h"
#include "LineTracker.h"

#include "Server.h"

int main()
{
#ifdef MSLP_DEBUG
    GapBuffer<char>::UnitTest();
    LineTracker::UnitTest();
#endif

    Server::Init();
    Server::Run();

    return 0;
}
