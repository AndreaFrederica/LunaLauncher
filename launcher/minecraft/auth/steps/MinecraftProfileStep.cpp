#include "MinecraftProfileStep.h"

#include <QNetworkRequest>

#include "Application.h"
#include "minecraft/auth/Parsers.h"
#include "minecraft/auth/LocalYggdrasil.h"
#include "net/NetUtils.h"
#include "net/RawHeaderProxy.h"

MinecraftProfileStep::MinecraftProfileStep(AccountData* data) : AuthStep(data) {}

QString MinecraftProfileStep::describe()
{
    return tr("Fetching the Minecraft profile.");
}

void MinecraftProfileStep::perform()
{
    QUrl url("https://api.minecraftservices.com/minecraft/profile");
    auto headers = QList<Net::HeaderPair>{ { "Content-Type", "application/json" },
                                           { "Accept", "application/json" },
                                           { "Authorization", QString("Bearer %1").arg(m_data->yggdrasilToken.token).toUtf8() } };

    auto [request, response] = Net::Download::makeByteArray(url);
    m_request = request;
    m_request->addHeaderProxy(std::make_unique<Net::RawHeaderProxy>(headers));
    m_request->enableAutoRetry(true);

    m_task.reset(new NetJob("MinecraftProfileStep", APPLICATION->network()));
    m_task->setAskRetry(false);
    m_task->addNetAction(m_request);

    connect(m_task.get(), &Task::finished, this, [this, response] { onRequestDone(response); });

    m_task->start();
}

void MinecraftProfileStep::onRequestDone(QByteArray* response)
{
    if (m_request->error() == QNetworkReply::ContentNotFoundError) {
        LocalYggdrasil::instance().removeAccountSnapshot(m_data->profileId());
        // NOTE: Succeed even if we do not have a profile. This is a valid account state.
        m_data->minecraftProfile = MinecraftProfile();
        emit finished(AccountTaskState::STATE_WORKING, tr("Account has no Minecraft profile."));
        return;
    }
    if (m_request->error() != QNetworkReply::NoError) {
        qWarning() << "Error getting profile:";
        qWarning() << " HTTP Status       :" << m_request->replyStatusCode();
        qWarning() << " Internal error no.:" << m_request->error();
        qWarning() << " Error string      :" << m_request->errorString();

        qWarning() << " Response:";
        qWarning() << QString::fromUtf8(*response);

        if (m_request->replyStatusCode() == 401 || m_request->replyStatusCode() == 403) {
            emit finished(AccountTaskState::STATE_FAILED_HARD, tr("Minecraft authentication was rejected."));
        } else if (Net::isApplicationError(m_request->error()) && !Net::isServerError(m_request->error())) {
            emit finished(AccountTaskState::STATE_FAILED_SOFT,
                          tr("Minecraft Java profile acquisition failed: %1").arg(m_request->errorString()));
        } else {
            m_data->networkError = m_request->error();
            emit finished(AccountTaskState::STATE_OFFLINE,
                          tr("Minecraft Java profile acquisition failed: %1").arg(m_request->errorString()));
        }
        return;
    }
    MinecraftProfile profile;
    if (!Parsers::parseMinecraftProfile(*response, profile)) {
        emit finished(AccountTaskState::STATE_FAILED_SOFT, tr("Minecraft Java profile response could not be parsed"));
        return;
    }
    if (profile.id == m_data->minecraftProfile.id) {
        if (profile.skin.url == m_data->minecraftProfile.skin.url)
            profile.skin.data = m_data->minecraftProfile.skin.data;
        for (auto it = profile.capes.begin(); it != profile.capes.end(); ++it) {
            const auto old = m_data->minecraftProfile.capes.constFind(it.key());
            if (old != m_data->minecraftProfile.capes.cend() && it->url == old->url)
                it->data = old->data;
        }
    }
    m_data->minecraftProfile = profile;
    LocalYggdrasil::instance().restoreAccountSnapshot(*m_data);

    emit finished(AccountTaskState::STATE_WORKING, tr("Got Minecraft profile"));
}
