#include "EntitlementsStep.h"

#include <QList>
#include <QNetworkRequest>
#include <QUrl>
#include <QUuid>
#include <memory>

#include "Application.h"
#include "Logging.h"
#include "minecraft/auth/Parsers.h"
#include "net/Download.h"
#include "net/NetJob.h"
#include "net/NetUtils.h"
#include "net/RawHeaderProxy.h"
#include "tasks/Task.h"

EntitlementsStep::EntitlementsStep(AccountData* data) : AuthStep(data) {}

QString EntitlementsStep::describe()
{
    return tr("Determining game ownership.");
}

void EntitlementsStep::perform()
{
    m_entitlements_request_id = QUuid::createUuid().toString(QUuid::WithoutBraces);

    QUrl url("https://api.minecraftservices.com/entitlements/license?requestId=" + m_entitlements_request_id);
    auto headers = QList<Net::HeaderPair>{ { "Content-Type", "application/json" },
                                           { "Accept", "application/json" },
                                           { "Authorization", QString("Bearer %1").arg(m_data->yggdrasilToken.token).toUtf8() } };

    auto [request, response] = Net::Download::makeByteArray(url);
    m_request = request;
    m_request->addHeaderProxy(std::make_unique<Net::RawHeaderProxy>(headers));
    m_request->enableAutoRetry(true);

    m_task.reset(new NetJob("EntitlementsStep", APPLICATION->network()));
    m_task->setAskRetry(false);
    m_task->addNetAction(m_request);

    connect(m_task.get(), &Task::finished, this, [this, response] { onRequestDone(response); });

    m_task->start();
    qDebug() << "Getting entitlements...";
}

void EntitlementsStep::onRequestDone(QByteArray* response)
{
    qCDebug(authCredentials()) << *response;

    if (m_request->error() != QNetworkReply::NoError) {
        const auto status = m_request->replyStatusCode();
        auto state = status >= 500 || !Net::isApplicationError(m_request->error()) ? AccountTaskState::STATE_OFFLINE
                                                                                   : AccountTaskState::STATE_FAILED_SOFT;
        if (status == 401 || status == 403)
            state = AccountTaskState::STATE_FAILED_HARD;
        emit finished(state, tr("Could not verify game ownership: %1").arg(m_request->errorString()));
        return;
    }
    MinecraftEntitlement entitlement;
    if (!Parsers::parseMinecraftEntitlements(*response, entitlement)) {
        emit finished(AccountTaskState::STATE_FAILED_SOFT, tr("Could not parse game ownership response."));
        return;
    }
    m_data->minecraftEntitlement = entitlement;

    emit finished(AccountTaskState::STATE_WORKING, tr("Got entitlements"));
}
