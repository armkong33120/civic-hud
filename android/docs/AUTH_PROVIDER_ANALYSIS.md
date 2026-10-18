# Authentication provider analysis

Research date: 18 October 2026. Official documentation and public discovery metadata only. No cloud resources, provider configuration, app source or phone state were changed. Existing tenant/project/client availability has not been established by this research; the root agent's inventory and the owner's answer are authoritative. A supported architecture is not evidence of functioning OAuth.

## Recommendation

Prefer **Microsoft Entra External ID in an external customer tenant**, using browser-delegated Android authentication, if the owner already has a suitable tenant/project. Use its built-in Google/Facebook and email options, and run a short **LINE custom-OIDC compatibility test** before committing to LINE federation. Keep an application-owned account ID and a server-side identity mapping so later linking does not depend on matching email addresses or on merging directory users.

This recommendation is conditional: the reviewed documentation establishes a plausible protocol intersection for LINE, not a tested Entra–LINE integration. If that test fails, consider Firebase Authentication; a LINE token bridge hosted on Azure is a fallback, not a prerequisite for the Entra design. Do not start a new Azure AD B2C deployment for this app: Microsoft says B2C stopped being available for new customers to purchase on 1 May 2025. [Microsoft deployment planning](https://learn.microsoft.com/en-us/entra/external-id/customers/concept-planning-your-solution)

## Entra capabilities and provider configuration

External ID customer tenants support email/password or email OTP, Google/Facebook federation and custom OIDC. Social/federated providers require **browser-delegated authentication**; the native authentication option supports local email methods. A Compose account screen can launch this browser flow, but a completely native social-password form is not the supported architecture. [Microsoft identity-provider overview](https://learn.microsoft.com/en-us/entra/external-id/customers/concept-authentication-methods-customers)

| Method | Configuration needed | Release/verification boundary |
| --- | --- | --- |
| Email | External customer tenant, app registration, associated sign-up/sign-in flow, chosen email method and recovery configuration | Test registration, verification, recovery, cancellation and relaunch; do not implement a local imitation of a cloud account. |
| Google | Google Cloud OAuth consent configuration and a **web** OAuth client for Entra federation; client ID/secret stored with the identity provider; exact tenant callback URLs; enable Google in the user flow | A separate Android app registration/redirect still connects Android to Entra. Google consent publishing/verification requirements depend on app configuration. [Microsoft Google setup](https://learn.microsoft.com/en-gb/entra/external-id/customers/how-to-google-federation-customers) |
| Facebook | Meta app with Facebook Login, app ID/secret, tenant callback URLs, requested email permission and enabled user-flow provider | Development access is limited; complete Meta's applicable go-live/data-handling/business requirements. Privacy, terms and data-deletion URLs are configuration inputs. [Microsoft Facebook setup](https://learn.microsoft.com/en-us/entra/external-id/customers/how-to-facebook-federation-customers) |
| LINE | LINE Login channel, custom OIDC provider, registered broker callbacks, secret held by the broker and mapped subject/name claims | Compatible settings exist below; signing validation and real sign-in still need a tenant/channel test. |

For Android, MSAL documents public-client configuration with package/signature-bound redirect handling. Use the SDK's browser/Custom Tab approach, register actual debug/release signing identities as appropriate and keep provider secrets out of the APK. [MSAL Android](https://learn.microsoft.com/en-us/entra/msal/android/), [browser behavior](https://learn.microsoft.com/en-us/entra/msal/android/customize-browsers-and-webviews)

## LINE custom-OIDC compatibility

Public metadata was fetched read-only from [LINE's discovery document](https://access.line.me/.well-known/openid-configuration). Its current values include:

| Setting | LINE metadata |
| --- | --- |
| Issuer | `https://access.line.me` |
| Authorization | `https://access.line.me/oauth2/v2.1/authorize` |
| Token | `https://api.line.me/oauth2/v2.1/token` |
| JWKS | `https://api.line.me/oauth2/v2.1/certs` |
| Response / subject | `code` / `pairwise` |
| ID-token signing | `ES256` |
| Token authentication | `client_secret_basic`, `client_secret_post` |
| PKCE | `S256` |
| Scopes | `openid`, `profile`, `email` |

Entra's current custom-OIDC article supports authorization-code response and `client_secret_post`/`client_secret_jwt`; it excludes `client_secret_basic` and warns that displayed `private_key_jwt` is unsupported. Therefore use **code + client_secret_post** for the proposed federation. Configure the two exact tenant federation callbacks documented by Microsoft, the LINE channel ID/secret and claim mappings. The article does not explicitly resolve LINE's ES256 token compatibility; that is a required test, not an assumed success.

Email is required by default, but the Microsoft article updated 15 October 2026 now documents making email optional at the **user-flow level** through Graph configuration. That affects all apps sharing that flow. Do not invent placeholder email addresses for LINE users. [Microsoft custom OIDC configuration](https://learn.microsoft.com/en-us/entra/external-id/customers/how-to-custom-oidc-federation-customers)

LINE requires channel-level email-access permission before requesting the email scope, and the user may decline it. Start with `openid profile` if email is unnecessary and the chosen Entra flow allows it. LINE supports state, nonce, login prompts and PKCE in its authorization flow; callbacks must match channel settings. The broker is the web OAuth client here; this is separate from directly embedding LINE Login's web flow into the native app. [LINE web integration](https://developers.line.biz/en/docs/line-login/integrate-line-login/)

Where a custom backend consumes LINE tokens, verify them on the server: signature or LINE's verification endpoint, expected channel audience, issuer, expiry and transaction nonce. Never trust an Android-supplied LINE user ID or decoded JWT payload alone. LINE's documentation describes ES256/JWKS and HS256/channel-secret verification cases; the fetched public discovery currently advertises ES256, so do not assume every integration returns the same token algorithm. [LINE token verification](https://developers.line.biz/en/docs/line-login/verify-id-token/)

## Linking identities safely

Microsoft Graph describes associated sign-in identities by sign-in type, issuer and issuer-assigned ID. That model does not itself supply a safe end-user linking flow. No turnkey External ID customer-account-linking journey was verified in this research. Treat any directory-level linking implementation as a separate supported-API investigation; do not copy legacy B2C custom-policy recipes into External ID without validation. [Microsoft Graph objectIdentity](https://learn.microsoft.com/en-us/graph/api/resources/objectidentity?view=graph-rest-1.0)

**Recommended application design** (engineering recommendation, not a claim of an existing product feature):

1. Assign an immutable `appAccountId`; keep vehicle/history ownership on that ID. Map each independently validated broker identity to it, using stable issuer/subject or tenant/object identifiers appropriate to the validated token. Email and display name are profile fields.
2. In Settings → Linked sign-in methods, require recent authentication for the current account and fresh proof of the identity being added. Bind both proofs to one short-lived, single-use server transaction, with state/nonce and audience validation. Use provider-supported reauthentication; if sufficient recent proof cannot be established, do not complete linking.
3. Add the mapping transactionally with a uniqueness constraint. A provider identity already attached to a different app account requires proof of both accounts and an explicit data-merge decision. Never silently overwrite ownership or merge by equal email, including equal verified emails.
4. Protect unlinking with reauthentication; retain at least one usable method or require a recovery method. Signing out, unlinking and deleting an account are distinct actions. Existing local vehicle history stays intact unless the user explicitly chooses an import, merge or deletion operation.

The backend can map two separate Entra customer identities to one app account without claiming to have merged their directory objects. Test sign-in through every linked identity after reinstall/restart, and test collision, cancellation, replay, expired proof and unlinking of the final method.

## Firebase alternative, only if needed

Firebase has Android support for email, Google and Facebook. Its documented `linkWithCredential` flow links provider credentials to the signed-in user's Firebase UID; a credential already owned by another UID needs application handling. The current documentation also flags a known linking issue in some projects, so verify the selected SDK/project path. [Firebase account linking](https://firebase.google.com/docs/auth/android/account-linking)

Before building a LINE custom-token bridge, evaluate Firebase's generic OIDC option: it requires an **Identity Platform upgrade** and still needs a LINE configuration/compatibility test. This may avoid custom token infrastructure. [Firebase Android OIDC](https://firebase.google.com/docs/auth/android/openid-connect)

If custom authentication is required, an Azure-hosted endpoint can validate LINE proof, resolve the canonical mapping and create a Firebase custom token for the correct UID. Firebase documents server-side token creation using a Google service-account signing identity; signing credentials belong on the server, never Android. [Firebase custom tokens](https://firebase.google.com/docs/auth/admin/create-custom-tokens)

A custom token is a sign-in mechanism, not a LINE `AuthCredential` that can simply be passed to `linkWithCredential`. The bridge needs its own authenticated link endpoint and uniqueness checks. Do not accept an arbitrary client-provided UID to mint a token. This alternative adds a second identity platform plus Azure backend/service-account operations; it is justified only by a concrete Entra blocker or an already-owned Firebase project.

## Account deletion and Play release

If the app allows account creation, Google Play requires an in-app account/data deletion path and a functional external web resource for deletion requests. Disabling or freezing the account alone is insufficient. Declare any justified retention clearly, and complete Play's Data safety/deletion answers. These obligations apply even if some app features remain available without signing in. [Google Play account deletion requirements](https://support.google.com/googleplay/android-developer/answer/13327111?hl=en)

Plan backend deletion across app account, linked mappings, cloud copies and feedback attachments; explain the separate choice for device-local vehicle/history data. Deleting the app account must not claim to delete the user's Google/LINE/Facebook account. Review the broker's dedicated/shared-user lifecycle before deleting directory objects used by other apps. Password recovery, reauthentication and deletion should be reviewed as part of authentication, not deferred until publication.

## Readiness check before calling OAuth functional

- Confirm the owner-approved external tenant or Firebase project, administrative access and billing/production intent; an Azure CLI login alone proves none of these.
- Record nonsecret app/client/channel identifiers, exact redirect configuration and provider/user-flow enablement. Store secrets only in the broker/backend secret store.
- Prove each real provider's sign-in, cancel, denied consent, sign-out and return-to-app flow on the actual build. Prove LINE ID-token signature compatibility specifically.
- Prove linking with both fresh identities, rejection of email-only correlation, collision handling and protection against losing the final method.
- Complete privacy/deletion URLs and tested deletion behavior before Play release. Until these checks pass, provider buttons must be labeled unavailable/configuration pending rather than simulate successful accounts.

Current result: architecture researched; LINE protocol candidate identified; **tenant/project availability and every live OAuth flow remain unverified in this research deliverable**.
