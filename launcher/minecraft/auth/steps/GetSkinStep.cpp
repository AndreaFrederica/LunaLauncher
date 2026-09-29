
#include "GetSkinStep.h"

#include <QNetworkRequest>

#include "Application.h"
#include "minecraft/auth/LocalYggdrasil.h"

GetSkinStep::GetSkinStep(AccountData* data) : AuthStep(data) {}

QString GetSkinStep::describe()
{
    return tr("Getting skin.");
}

void GetSkinStep::perform()
{
    m_task.reset(new NetJob("GetSkinStep", APPLICATION->network()));
    m_task->setAskRetry(false);
    auto addTexture = [this](const QString& source, const QString& capeId) {
        if (source.isEmpty())
            return;
        auto url = QUrl(source);
        if (m_data->type == AccountType::MSA)
            url = LocalYggdrasil::instance().textureUrl(url);
        auto [request, response] = Net::Download::makeByteArray(url);
        auto* download = request.get();
        connect(download, &Task::finished, this, [this, download, response, capeId] {
            if (download->error() != QNetworkReply::NoError)
                return;
            if (capeId.isEmpty())
                m_data->minecraftProfile.skin.data = *response;
            else
                m_data->minecraftProfile.capes[capeId].data = *response;
        });
        m_task->addNetAction(request);
    };
    addTexture(m_data->minecraftProfile.skin.url, {});
    if (m_data->type == AccountType::MSA || m_data->type == AccountType::Yggdrasil) {
        const auto cape = m_data->minecraftProfile.capes.constFind(m_data->minecraftProfile.currentCape);
        if (cape != m_data->minecraftProfile.capes.cend())
            addTexture(cape->url, cape.key());
    }
    connect(m_task.get(), &Task::finished, this, [this] { emit finished(AccountTaskState::STATE_WORKING, tr("Got skin")); });
    if (m_task->size() == 0) {
        emit finished(AccountTaskState::STATE_WORKING, tr("No skin available"));
        return;
    }
    m_task->start();
}
