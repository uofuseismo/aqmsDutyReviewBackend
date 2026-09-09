#ifndef AQMS_DUTY_REVIEW_BACKEND_DATABASE_AQMS_CATALOG_CACHE_HPP
#define AQMS_DUTY_REVIEW_BACKEND_DATABASE_AQMS_CATALOG_CACHE_HPP
#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <boost/json/object.hpp>

namespace AQMSDutyReviewBackend::Database::AQMS
{
/// @class CatalogCache catalogCache.hpp
/// @brief Holds the serialized catalog so that asking whether it changed
///        does not rebuild it.
///
/// The catalog query is a five-table join costing about 275 ms; the
/// freshness token that says whether it moved costs about 30 ms.  The
/// frontend polls the hash endpoint, and without this every poll paid the
/// 275 ms to compute a hash the server had already computed moments
/// earlier.
///
/// The catalog barely moves - roughly twice an hour in the archive - so nearly
/// every poll is a hit.
///
/// @note Knows nothing about the database.  It is handed a token and an
///       already-serialized catalog, which is what lets it be tested
///       without one, and what keeps the decision of what "changed" means
///       in the query where the evidence for it is.
///
/// @note Thread safe.  Crow currently runs one thread, but a cache that
///       silently required that would be an unpleasant surprise the day
///       numberOfThreads changes.
///
/// @copyright Ben Baker (University of Utah) distributed under the
///            MIT NO AI license.
class CatalogCache
{
public:
    /// @brief What was cached.
    struct Entry
    {
        /// The catalog as it goes on the wire - {events: [...], hash: ...}.
        boost::json::object catalog;
        /// The same hash, on its own, for the hash endpoint.
        std::string hash;
    };
public:
    /// @brief Constructor.
    /// @param[in] maximumAge  How long an entry may be served after it was
    ///                        stored, however unchanged the token looks.
    ///
    /// @note A BACKSTOP, not the mechanism.  The window sliding used to be
    ///       invisible to the token, and this was what caught it; the
    ///       token now carries the window bucket, so a slide changes the
    ///       token and the age limit has nothing left to catch.  It stays
    ///       because a cache with no upper bound on staleness is a thing
    ///       nobody wants to discover they have.
    ///
    /// @note Five minutes by default, matching the window bucket, so it
    ///       never expires an entry the token would have kept.
    explicit CatalogCache(
        const std::chrono::seconds &maximumAge = std::chrono::minutes {5});

    /// @brief Looks the catalog up.
    /// @param[in] token  The freshness token the catalog would be built
    ///                   at now.
    /// @result The cached catalog when the token matches what it was
    ///         stored with AND the entry is not older than the maximum
    ///         age; nullopt otherwise, meaning the caller must rebuild.
    [[nodiscard]] std::optional<Entry> lookup(const std::string &token) const;

    /// @brief Stores a freshly built catalog against the token it was
    ///        built at.
    /// @note Take the token BEFORE building, not after.  A token read
    ///       afterwards could belong to a state the catalog in hand does
    ///       not reflect, and would then be served as though it did.
    void store(const std::string &token,
               const boost::json::object &catalog,
               const std::string &hash);

    /// @brief Forgets whatever is held.
    void clear();

    /// @result How many lookups have been served from the cache, and how
    ///         many have had to rebuild.
    [[nodiscard]] std::pair<int64_t, int64_t> getHitsAndMisses() const noexcept;

    /// @brief Destructor.
    ~CatalogCache();

    CatalogCache(const CatalogCache &) = delete;
    CatalogCache(CatalogCache &&) noexcept = delete;
    CatalogCache& operator=(const CatalogCache &) = delete;
    CatalogCache& operator=(CatalogCache &&) noexcept = delete;
private:
    class CatalogCacheImpl;
    std::unique_ptr<CatalogCacheImpl> pImpl;
};
}
#endif
