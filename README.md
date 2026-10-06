# Teamcenter → SAP Product Transfer Sample

This repository contains a reference implementation for transferring an approved Teamcenter Item/Item Revision into an SAP S/4HANA Product Master.

## Architecture

Teamcenter ITK → extract Item / latest Revision → map Teamcenter attributes to SAP Product Master → SAP OData V2 `API_PRODUCT_SRV/A_Product`

The sample keeps the SAP endpoint and credentials configurable and does not assume that every Teamcenter installation has the same custom properties.

## Files

- `sample/item_to_sap/teamcenter_sap_transfer.cxx` — Teamcenter ITK C++ example. Finds an Item, obtains its latest revision, reads standard properties, builds an SAP product payload, obtains a CSRF token, and POSTs the product.
- `sample/item_to_sap/sap_product_payload.json` — example payload for Postman/curl.
- `sample/item_to_sap/config.example.env` — configuration template.

## SAP target

`POST <SAP_HOST>/sap/opu/odata/sap/API_PRODUCT_SRV/A_Product`

SAP documents `API_PRODUCT_SRV` as the Product Master (A2X) OData V2 API and documents POST creation on `A_Product`. Product descriptions can be supplied through `to_Description`.

## Important production changes

1. Prefer OAuth/client-certificate authentication or your enterprise-approved SAP authentication mechanism instead of Basic Authentication.
2. Put Teamcenter-to-SAP mapping in a dedicated mapping layer or configuration rather than hard-coding business rules.
3. Add an idempotency/correlation ID and persist the SAP material/product number returned by SAP.
4. Validate mandatory SAP fields for the specific product type, plant, valuation area, and company configuration.
5. Consider SAP Integration Suite/CPI, a message broker, or another integration layer when enterprise architecture requires asynchronous retry, monitoring, transformation, and dead-letter handling.
6. Do not log passwords, cookies, CSRF tokens, or full SAP payloads if they contain sensitive data.

## Example flow

1. A Teamcenter Item Revision reaches the business status that means “released for SAP”.
2. An ITK job receives the Teamcenter Item ID.
3. The latest revision is loaded.
4. Teamcenter values are mapped to an SAP Product.
5. The integration obtains an SAP CSRF token.
6. The integration sends POST `A_Product`.
7. SAP returns the created Product/material identifier.
8. The integration records that identifier in Teamcenter or an integration status store.

The C++ sample is a starting point and must be compiled against the Teamcenter ITK libraries and libcurl available in your environment.
