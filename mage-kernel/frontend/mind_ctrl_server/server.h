#include <mind_ctrl_types.h>
#include <MindCtrl.h>

class MindCtrlHandler : public mind_ctrl::MindCtrlIf {
public:
    MindCtrlHandler() = default;
    void exchange(mind_ctrl::Command& _return, const mind_ctrl::Command& cmd) override;
};

void start_server();
