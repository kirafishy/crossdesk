-- Python's recipe looks up its dependency as "openssl". Keep that name while
-- using the OpenSSL 3 recipe, including its sources, patches and build scripts.
package("openssl")
    set_base("openssl3")
    -- Rebuild consumers cached against the old OpenSSL 1.x ABI.
    set_policy("package.strict_compatibility", true)
package_end()
