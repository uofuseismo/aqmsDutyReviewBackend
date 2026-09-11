#include <chrono>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include "aqmsDutyReviewBackend/database/aqms/catalogCache.hpp"

using namespace AQMSDutyReviewBackend::Database::AQMS;

/// Why this exists: the catalog query is a five-table join costing a few
/// hundred milliseconds, while the freshness token that says whether it
/// moved costs tens.  The frontend polls the hash endpoint, so without a
/// cache every poll rebuilt a catalog to recompute a hash the server had
/// produced moments earlier.
///
/// Why the maximum age is a BACKSTOP rather than the mechanism: the
/// catalog covers a rolling window, so events age out of it with no lddate
/// moving anywhere.  That used to be invisible to the token and this was
/// what caught it.  The token now carries the window bucket and catches it
/// itself, so the age limit has little left to do - it stays because a
/// cache with no upper bound on staleness is a thing nobody wants to
/// discover they have.  The default matches the window bucket, so it never
/// expires an entry the token would have kept.

class CatalogCache::CatalogCacheImpl
{
public:
    mutable std::mutex mMutex;
    std::chrono::seconds mMaximumAge{std::chrono::minutes {5}};
    std::string mToken;
    boost::json::object mCatalog;
    std::string mHash;
    /// steady_clock, not system_clock: this measures how long the entry
    /// has been held, and a wall clock that steps backwards would make an
    /// old entry look new.
    std::chrono::steady_clock::time_point mStoredAt;
    int64_t mHits{0};
    int64_t mMisses{0};
    bool mHasEntry{false};
};

CatalogCache::CatalogCache(const std::chrono::seconds &maximumAge) :
    pImpl(std::make_unique<CatalogCacheImpl> ())
{
    if (maximumAge.count() > 0){pImpl->mMaximumAge = maximumAge;}
}

std::optional<CatalogCache::Entry>
CatalogCache::lookup(const std::string &token) const
{
    const std::lock_guard<std::mutex> lock{pImpl->mMutex};
    if (!pImpl->mHasEntry || pImpl->mToken != token)
    {
        pImpl->mMisses = pImpl->mMisses + 1;
        return std::nullopt;
    }
    const auto age = std::chrono::steady_clock::now() - pImpl->mStoredAt;
    if (age > pImpl->mMaximumAge)
    {
        // The token can be identical and the catalog still wrong: the
        // window it covers is rolling, so events leave it without
        // touching a thing.
        pImpl->mMisses = pImpl->mMisses + 1;
        return std::nullopt;
    }
    pImpl->mHits = pImpl->mHits + 1;
    return Entry {pImpl->mCatalog, pImpl->mHash};
}

void CatalogCache::store(const std::string &token,
                         const boost::json::object &catalog,
                         const std::string &hash)
{
    const std::lock_guard<std::mutex> lock{pImpl->mMutex};
    pImpl->mToken = token;
    pImpl->mCatalog = catalog;
    pImpl->mHash = hash;
    pImpl->mStoredAt = std::chrono::steady_clock::now();
    pImpl->mHasEntry = true;
}

void CatalogCache::clear()
{
    const std::lock_guard<std::mutex> lock{pImpl->mMutex};
    pImpl->mHasEntry = false;
    pImpl->mToken.clear();
    pImpl->mCatalog = boost::json::object {};
    pImpl->mHash.clear();
}

std::pair<int64_t, int64_t> CatalogCache::getHitsAndMisses() const noexcept
{
    const std::lock_guard<std::mutex> lock{pImpl->mMutex};
    return std::pair {pImpl->mHits, pImpl->mMisses};
}

CatalogCache::~CatalogCache() = default;
