// Copyright 2014 Stellar Development Foundation and contributors. Licensed
// under the Apache License, Version 2.0. See the COPYING file at the root
// of this distribution or at http://www.apache.org/licenses/LICENSE-2.0

#include "transactions/SetOptionsOpFrame.h"
#include "crypto/Base58.h"
#include "database/Database.h"

#include "medida/meter.h"
#include "medida/metrics_registry.h"

namespace stellar
{
SetOptionsOpFrame::SetOptionsOpFrame(Operation const& op, OperationResult& res,
                                     TransactionFrame& parentTx)
    : OperationFrame(op, res, parentTx)
    , mSetOptions(mOperation.body.setOptionsOp())
{
}

int32_t
SetOptionsOpFrame::getNeededThreshold() const
{
    // updating thresholds or signer requires high threshold
    if (mSetOptions.thresholds || mSetOptions.signer)
    {
        return mSourceAccount->getHighThreshold();
    }
    return mSourceAccount->getMediumThreshold();
}

bool
SetOptionsOpFrame::doApply(medida::MetricsRegistry& metrics,
                           LedgerDelta& delta, LedgerManager& ledgerManager)
{
    Database& db = ledgerManager.getDatabase();
    AccountEntry& account = mSourceAccount->getAccount();

    if (mSetOptions.inflationDest)
    {
        AccountFrame::pointer inflationAccount;
        AccountID inflationID = *mSetOptions.inflationDest;
        inflationAccount = AccountFrame::loadAccount(inflationID, db);
        if (!inflationAccount)
        {
            metrics.NewMeter({"op-set-options", "failure",
                              "invalid-inflation"},
                             "operation").Mark();
            innerResult().code(SET_OPTIONS_INVALID_INFLATION);
            return false;
        }
        account.inflationDest.activate() = inflationID;
    }

    if (mSetOptions.clearFlags)
    {
        account.flags = account.flags & ~*mSetOptions.clearFlags;
    }
    if (mSetOptions.setFlags)
    {
        if ((*mSetOptions.setFlags & AUTH_REQUIRED_FLAG) ||
            (*mSetOptions.setFlags & AUTH_REVOCABLE_FLAG))
        {
            // must ensure no one is holding your credit
            if (TrustFrame::hasIssued(account.accountID, db))
            {
                metrics.NewMeter({"op-set-options", "failure",
                                  "cant-change"},
                                 "operation").Mark();
                innerResult().code(SET_OPTIONS_CANT_CHANGE);
                return false;
            }
        }
        account.flags = account.flags | *mSetOptions.setFlags;
    }

    if (mSetOptions.homeDomain)
    {
        account.homeDomain = *mSetOptions.homeDomain;
    }

    if (mSetOptions.thresholds)
    {
        account.thresholds = *mSetOptions.thresholds;
    }

    if (mSetOptions.signer)
    {
        auto& signers = account.signers;
        if (mSetOptions.signer->weight)
        { // add or change signer
            bool found = false;
            for (auto& oldSigner : signers)
            {
                if (oldSigner.pubKey == mSetOptions.signer->pubKey)
                {
                    oldSigner.weight = mSetOptions.signer->weight;
                }
            }
            if (!found)
            {
                if (signers.size() == signers.max_size())
                {
                    metrics.NewMeter({"op-set-options", "failure",
                                      "too-many-signers"},
                                     "operation").Mark();
                    innerResult().code(SET_OPTIONS_TOO_MANY_SIGNERS);
                    return false;
                }
                if (!mSourceAccount->addNumEntries(1, ledgerManager))
                {
                    metrics.NewMeter({"op-set-options", "failure",
                                      "low-reserve"},
                                     "operation").Mark();
                    innerResult().code(SET_OPTIONS_LOW_RESERVE);
                    return false;
                }
                signers.push_back(*mSetOptions.signer);
            }
        }
        else
        { // delete signer
            auto it = signers.begin();
            while (it != signers.end())
            {
                Signer& oldSigner = *it;
                if (oldSigner.pubKey == mSetOptions.signer->pubKey)
                {
                    it = signers.erase(it);
                    mSourceAccount->addNumEntries(-1, ledgerManager);
                }
                else
                {
                    it++;
                }
            }
        }
        mSourceAccount->setUpdateSigners();
    }

    metrics.NewMeter({"op-set-options", "success", "apply"},
                     "operation").Mark();
    innerResult().code(SET_OPTIONS_SUCCESS);
    mSourceAccount->storeChange(delta, db);
    return true;
}

bool
SetOptionsOpFrame::doCheckValid(medida::MetricsRegistry& metrics)
{
    if (mSetOptions.setFlags && mSetOptions.clearFlags)
    {
        if ((*mSetOptions.setFlags & *mSetOptions.clearFlags) != 0)
        {
            metrics.NewMeter({"op-set-options", "invalid", "bad-flags"},
                             "operation").Mark();
            innerResult().code(SET_OPTIONS_BAD_FLAGS);
            return false;
        }
    }
    return true;
}
}
