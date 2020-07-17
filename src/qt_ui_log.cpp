#include "qt_ui_log.h"

#include "SSThread.hpp"

void send_traffic_stat(uint64_t tx, uint64_t rx)
{
    auto ptr = dynamic_cast<SSThread*>(QThread::currentThread());
    if (!ptr)
        return;
    emit ptr->OnDataReady(tx, rx);
}
void qt_ui_log(const char* msg)
{
    auto ptr = dynamic_cast<SSThread*>(QThread::currentThread());
    if (!ptr)
        return;
    emit ptr->onSSRThreadLog(QString { msg });
}
