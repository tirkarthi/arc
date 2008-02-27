#ifndef __ARC_LDAPQUERY_H__
#define __ARC_LDAPQUERY_H__

#include <ldap.h>

#include <list>
#include <string>
#include <list>

#include <arc/Logger.h>
#include <arc/URL.h>

#define SASLMECH "GSI-GSSAPI"
#define TIMEOUT 10

/**
 * LDAP callback type. Your ldap callbacks should be of same structure.
 */
typedef void (*ldap_callback)(const std::string& attr,
                              const std::string& value,
                              void *ref);

namespace Arc {

/**
 *  LDAPQuery class; querying of LDAP servers.
 */
class LDAPQuery {

	public:
		/**
		 * Constructs a new LDAPQuery object and sets connection options.
		 * The connection is first established when calling Query.
		 */
		LDAPQuery(const std::string& ldaphost,
		          int ldapport,
		          bool anonymous = true,
		          const std::string& usersn = "",
		          int timeout = TIMEOUT);

		/**
		 * Destructor. Will disconnect from the ldapserver if stll connected.
		 */
		~LDAPQuery();

		/**
		 * Scope for a LDAP queries. Use when querying.
		 */
		enum Scope { base, onelevel, subtree };

		/**
		 * Queries the ldap server.
		 */
		bool Query(const std::string& base,
		           const std::string& filter = "(objectclass=*)",
		           const std::list <std::string>& attributes =
				       std::list<std::string>(),
		           Scope scope = subtree);

		/**
		 * Retrieves the result of the query from the ldap-server.
		 */
		bool Result(ldap_callback callback,
		            void *ref);

	private:
		bool Connect();
		bool SetConnectionOptions(int version);
		bool HandleResult(ldap_callback callback, void *ref);
		void HandleSearchEntry(LDAPMessage *msg,
		                       ldap_callback callback,
		                       void *ref);

		std::string host;
		int port;
		bool anonymous;
		std::string usersn;
		int timeout;

		ldap *connection;
		int messageid;

		static Logger logger;
};

} // end namespace

#endif // __ARC_LDAPQUERY_H__
