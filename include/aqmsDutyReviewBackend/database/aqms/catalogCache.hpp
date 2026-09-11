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
/// @brief Holds one serialized catalog against the freshness token it was
///        built at, so a caller can ask whether the catalog changed
///        without rebuilding it.
///
/// @note Knows nothing about the database.  It is given a token and an
///       already-serialized catalog; what a token means is the caller's
///       business.
/// @note Thread safe.
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
    ///                        An upper bound on staleness; the token is
    ///                        what normally decides.
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
    /// @warning The token must be the one read BEFORE the catalog was
    ///          built.  A token read afterwards may describe a state this
    ///          catalog does not reflect, and the entry will then be
    ///          served as though it did.
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
