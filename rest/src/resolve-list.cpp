#include "resolve-list.h"
#include "common/commands.h"
#include "common/message-bus.h"
#include "common/string.h"
#include "group-rest.h"
#include <asset/json.h>
#include <fty/rest/component.h>
#include <fty/string-utils.h>

#include <fty_log.h>
#include <fty_common_rest_audit_log.h>

namespace fty::agroup {

unsigned ResolveList::run()
{
    rest::User user(m_request);
    if (auto ret = checkPermissions(user.profile(), m_permissions); !ret) {
        throw rest::Error(ret.error());
    }

    fty::groups::MessageBus bus;
    if (auto res = bus.init(AgentName); !res) {
        throw rest::errors::Internal(res.error());
    }

    fty::commands::resolve::list::In in;

    auto strIdsPrt = m_request.queryArg<std::string>("ids");
    if (strIdsPrt && !strIdsPrt->empty()) {
        std::stringstream ss(*strIdsPrt);
        while (ss.good()) {
            std::string substr;
            getline(ss, substr, ',');
            if (!fty::groups::isNumeric(substr)) {
                throw rest::errors::Internal("Not a number");
            }
            in.ids.append(fty::convert<uint16_t>(substr));
        }
    }

    fty::groups::Message msg = message(fty::commands::resolve::list::Subject);
    msg.setData(*pack::json::serialize(in));

    auto ret = bus.send(fty::Channel, msg);
    if (!ret) {
        throw rest::errors::Internal(ret.error());
    }

    commands::resolve::list::Out data;
    auto info = pack::json::deserialize(ret->userData[0], data);
    if (!info) {
        throw rest::errors::Internal(info.error());
    }

    bool isFirst = true;
    m_reply << "[";
    for (const auto& it : data) {
        if (!isFirst) {
            m_reply << ",";
        }
        else {
            isFirst = false;
        }
        m_reply << "{";
        m_reply << "\"id\": " << std::to_string(it.id) << ",";
        m_reply << "\"assets\": [";
        std::vector<std::string> out;
        for (const auto& asset : it.assets) {
            std::string json = asset::getJsonAsset(fty::convert<uint32_t>(asset.id.value()));
            if (json.empty()) {
                throw rest::errors::Internal("Cannot build asset information");
            }
            out.push_back(json);
        }
        m_reply << fty::implode(out, ", ");
        m_reply << "]";
        m_reply << "}";
    }
    m_reply << "]";
    return HTTP_OK;
}

} // namespace fty::agroup

registerHandler(fty::agroup::ResolveList)