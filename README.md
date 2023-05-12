#  ![](docs/img/dxfeed_c_logo.svg)



This package provides access to **[dxFeed](https://www.dxfeed.com/)** market data.

[![Release](https://img.shields.io/github/v/release/dxFeed/dxfeed-c-api)](https://github.com/dxFeed/dxfeed-c-api/releases/latest)
[![License](https://img.shields.io/badge/license-MPL--2.0-orange)](https://github.com/dxFeed/dxfeed-c-api/blob/master/LICENSE)
[![Downloads](https://img.shields.io/github/downloads/dxFeed/dxfeed-c-api/total)](https://github.com/dxFeed/dxfeed-c-api/releases/latest)

## Table of Contents
- [Documentation](#documentation)
- [Dependencies](#dependencies)
- [How to build](#how-to-build)
  * [Windows](#windows)
  * [Linux](#linux)
  * [macOS](#macos)
  * [Android Termux](#android-termux)
- [Key features](#key-features)
  * [Event types](#event-types)
  * [Contracts](#contracts)
    * [Ticker](#ticker) 
    * [Stream](#stream) 
    * [History](#history)  
  * [Subscription creation](#subscription-creation)
  * [Listener attachment](#listener-attachment)
  * [Order sources](#order-sources)
- [Usage](#usage)
  * [Create connection](#create-connection)
  * [Create subscription](#create-subscription)
  * [Setting up contract type](#setting-up-contract-type)
  * [Setting up symbol](#setting-up-symbol)
  * [Setting up Order source](#setting-up-order-source)
  * [Quote subscription](#quote-subscription)
- [Samples](#samples)


## Documentation
Find useful information in self-service dxFeed Knowledge Base, or C API framework documentation:

- [dxFeed Knowledge Base](https://kb.dxfeed.com/index.html?lang=en)
  * [Getting started](https://kb.dxfeed.com/en/getting-started.html)
  * [Troubleshooting](https://kb.dxfeed.com/en/troubleshooting-guidelines.html)
  * [Market events](https://kb.dxfeed.com/en/data-model/dxfeed-api-market-events.html)
  * [Event delivery contracts](https://kb.dxfeed.com/en/data-model/model-of-event-publishing.html#event-delivery-contracts)
  * [dxFeed API event classes](https://kb.dxfeed.com/en/data-model/model-of-event-publishing.html#dxfeed-api-event-classes)
  * [Exchange codes](https://kb.dxfeed.com/en/data-model/exchange-codes.html)
  * [Order sources](https://kb.dxfeed.com/en/data-model/qd-model-of-market-events.html#UUID-858ebdb1-0127-8577-162a-860e97bfe408_para-idm53255963764388)
  * [Order book reconstruction](https://kb.dxfeed.com/en/data-model/dxfeed-order-book-reconstruction.html)
  * [Symbol guide](https://downloads.dxfeed.com/specifications/dxFeed-Symbol-Guide.pdf)
- [dxFeed С API framework Documentation](https://docs.dxfeed.com/c-api/index.html?_ga=2.205912859.947963636.1629966027-849088511.1627377760)
  * [Order scopes](https://docs.dxfeed.com/c-api/group__event-data-structures-order-spread-order.html#ga78939b26eeb8eb1798e348451db90093)
  * [Event flags](https://docs.dxfeed.com/c-api/group__event-data-structures-event-subscription-stuff.html#ga3b406a7d463b6cc5fc2e14f33990b103)
  * [Subscriptions](https://docs.dxfeed.com/c-api/group__c-api-basic-subscription-functions.html)


## Dependencies

- API
  - Boost.Locale.EncodingUtf (1.75)
  - [TOML11](https://github.com/ToruNiina/toml11) (3.6.0)
- Tests
  - [Catch2](https://github.com/catchorg/Catch2) (2.13.8)
- C++ Wrappers
  - [Args](https://github.com/Taywee/args) (6.2.2)
  - [fmt](https://github.com/fmtlib/fmt) (8.0.0)
  - [optional-lite](https://github.com/martinmoene/optional-lite) (3.1.1)
  - [string-view-lite](https://github.com/martinmoene/string-view-lite) (1.1.0)
  - [variant-lite](https://github.com/martinmoene/variant-lite) (1.1.0)



## How to build

### Windows

System requirements (for the libressl): [Visual C++ Redistributable 2015](https://www.microsoft.com/en-us/download/details.aspx?id=52685), [Visual Studio](https://visualstudio.microsoft.com/vs/).

Prerequisites: `git`, `cmake`, `ninja` or `make`, `clang` (mingw or VS) or `gcc` (mingw) or `visual studio`

```console
git clone https://github.com/dxFeed/dxfeed-c-api.git
cd dxfeed-c-api
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release -DDISABLE_TLS=ON -DTARGET_PLATFORM=x64 ..
cmake --build . --config Release
```

---

### Linux

Prerequisites: `git`, `cmake`, `ninja` or `make`, `clang` or `gcc` 

```console
git clone https://github.com/dxFeed/dxfeed-c-api.git
cd dxfeed-c-api
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release -DDISABLE_TLS=ON -DTARGET_PLATFORM=x64 ..
cmake --build . --config Release
```

---


### macOS

Prerequisites: `git`, `cmake`, `ninja` or `make`, `clang`

```console
git clone https://github.com/dxFeed/dxfeed-c-api.git
cd dxfeed-c-api
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release -DDISABLE_TLS=ON -DTARGET_PLATFORM=x64 ..
cmake --build . --config Release
```
---

### Android Termux

Prerequisites: `git`, `cmake`, `ninja`, `clang`

```console
git clone https://github.com/dxFeed/dxfeed-c-api.git
cd dxfeed-c-api
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release -DDISABLE_TLS=ON -DTARGET_PLATFORM=x64 ..
cmake --build . --config Release
```


## Key features

#### Event types

| #		| EventType			| Short description																				|Examples of original feed events	|Struct	|
| :----:|:------------------|:----------------------------------------------------------------------------------------------|:------|:----------|
| 1		|[Trade](https://kb.dxfeed.com/en/data-model/qd-model-of-market-events.html#trade-19593)	 			|The price and size of the last trade during regular trading hours, an overall day volume and day turnover						|Trade (last sale), trade conditions change messages, volume setting events, index value 		|[dxf_trade_t](https://docs.dxfeed.com/c-api/structdxf__trade__t.html)|
| 2		| [TradeETH](https://kb.dxfeed.com/en/data-model/qd-model-of-market-events.html#tradeeth-19593)			|The price and size of the last trade during extended trading hours, and the extended trading hours day volume and day turnover					|Trade (last sale), trade conditions change messages, volume setting events		|[dxf_trade_t](https://docs.dxfeed.com/c-api/structdxf__trade__t.html)|
| 3		|[TimeAndSale](https://kb.dxfeed.com/en/data-model/qd-model-of-market-events.html#timeandsale-19593)		|The trade or other market event with price that provides information about trades in a continuous time slice (unlike Trade events, which are supposed to provide a snapshot of the current last trade)															|Trade, index value|[dxf_time_and_sale_t](https://docs.dxfeed.com/c-api/structdxf__time__and__sale.html)|
| 4		|[Quote](https://kb.dxfeed.com/en/data-model/qd-model-of-market-events.html#quote-19593)				|The best bid and ask prices and other fields that may change with each quote. It represents the most recent information that is available about the best quote on the market																	|BBO Quote (bid/ask), regional Quote (bid/ask) |[dxf_quote_t](https://docs.dxfeed.com/c-api/structdxf__quote__t.html)|
| 5		|[Order](https://kb.dxfeed.com/en/data-model/dxfeed-api-market-events.html#orders)				|Depending on the **`scope`** flag it may be: composite BBO from the whole market **or** regional BBO from a particular exchange **or** aggregated information (e.g. *PLB - price level book*) **or** individual order (*FOD - full order depth*)																				|Regional Quote (bid/ask), depth (order books, price levels, market maker quotes), market depth|[dxf_order_t](https://docs.dxfeed.com/c-api/structdxf__order__t.html)|
| 6		|[SpreadOrder](https://kb.dxfeed.com/en/data-model/dxfeed-api-market-events.html#spreadorder)		|Multi-leg order															|Regional Quote (bid/ask), depth (order books, price levels, market maker quotes), market depth|[dxf_order_t](https://docs.dxfeed.com/c-api/structdxf__order__t.html)||
| 7		| Candle			|OHLCV candle																					|Charting aggregations|[dxf_candle_t](https://docs.dxfeed.com/c-api/structdxf__candle__t.html)|
| 8		|[Profile](https://kb.dxfeed.com/en/data-model/qd-model-of-market-events.html#profile-19593)			|The most recent information that is available about the traded security on the market																		|Instrument definition, trading halt/resume messages|[dxf_profile_t](https://docs.dxfeed.com/c-api/structdxf__profile.html)|
| 9		|[Summary](https://kb.dxfeed.com/en/data-model/qd-model-of-market-events.html#summary-19593)			|The most recent OHLC information about the trading session on the market														|OHLC setting events (trades, explicit hi/lo update messages, explicit summary messages)|[dxf_summary_t](https://docs.dxfeed.com/c-api/structdxf__summary.html)|
| 10	|[Greeks](https://kb.dxfeed.com/en/data-model/dxfeed-api-market-events.html#greeks)			|Differential values that show how the price of an option depends on other market parameters		|Greeks and Black-Scholes implied volatility|[dxf_greeks_t](https://docs.dxfeed.com/c-api/structdxf__greeks.html)|
| 11	|[TheoPrice](https://kb.dxfeed.com/en/data-model/dxfeed-api-market-events.html#theoprice)			|The theoretical option price					|Theoretical prices|[dx_theo_price_t](https://docs.dxfeed.com/c-api/structdx__theo__price.html)|
| 12	|[Underlying](https://kb.dxfeed.com/en/data-model/dxfeed-api-market-events.html#underlying)		|Calculated values that are available for an option underlying symbol based on the option prices on the market																|VIX methodology implied volatility, P/C ratios|[dxf_underlying_t](https://docs.dxfeed.com/c-api/structdxf__underlying.html)||
| 13	|[Series](https://kb.dxfeed.com/en/data-model/dxfeed-api-market-events.html#series)			|Properties of the underlying																	|VIX methodology implied volatility, P/C ratios|[dxf_series_t](https://docs.dxfeed.com/c-api/structdxf__series.html)|


|:information_source: CODE SAMPLE:  take a look at `event_type` usage in  [CommandLineSample](https://github.com/dxFeed/dxfeed-c-api/blob/master/samples/CommandLineSample/CommandLineSample.c#L791)|
| --- |

|:white_check_mark: READ MORE: [Events to be published for different feed types, feed types and events matrix](https://kb.dxfeed.com/en/data-model/model-of-event-publishing.html#events-to-be-published-for-different-feed-types)|
| --- |

---
#### Contracts

There are three types of delivery contracts: **`Ticker`**, **`Stream`** and **`History`**. You can set up the contract type by **[dx_event_subscr_flag](https://docs.dxfeed.com/c-api/group__event-data-structures-event-subscription-stuff.html#ga3b406a7d463b6cc5fc2e14f33990b103)** as a parameter of **[dxf_create_subscription_with_flags](https://docs.dxfeed.com/c-api/group__c-api-basic-subscription-functions.html#ga72d2af657437fb51a803daf55b4bbaf3)**.
  
##### Ticker
The main task of this contract is to reliably deliver the latest value for an event (for example, for the last trade of the selected symbol). Queued older events could be conflated to conserve bandwidth and resources.
 
##### Stream
A stream contract guaranteedly delivers a stream of events without conflation.

##### History
History contract first delivers a snapshot (set of previous events) for the specified time range (subject to limitations). After the snapshot is transmitted, the history contract delivers a stream of events.

|:information_source: NOTE: if **[dx_event_subscr_flag](https://docs.dxfeed.com/c-api/group__event-data-structures-event-subscription-stuff.html#ga3b406a7d463b6cc5fc2e14f33990b103)** is not set, the default contract type will be enabled.|
| --- |

Default contracts for events (*in most cases*):

| Ticker			| Stream			| History	|
|:-----------------:|:-----------------:|:---------:|
|Trade	 			|TimeAndSale		|Order		|
|TradeETH			|					|SpreadOrder|
|Quote				|					|Candle		|
|Profile			|					|Series		|
|Summary			|					|Greeks		|
|Greeks				|					|			|
|TheoPrice			|					|			|
|Underlying			|					|			|	


|:information_source: CODE SAMPLE: take a look at `dx_event_subscr_flag` usage in [CommandLineSample](https://github.com/dxFeed/dxfeed-c-api/blob/master/samples/CommandLineSample/CommandLineSample.c#L786)|
| --- |

|:white_check_mark: READ MORE: [Event delivery contracts](https://kb.dxfeed.com/en/data-model/model-of-event-publishing.html#event-delivery-contracts)|
| --- |

---

#### Subscription creation


|#		|Method				|Handle			| Description		| Code sample|
|:-----:|:------------------|:--------------------------|:------------------|:-----------|
|1|[dxf_create_subscription](https://docs.dxfeed.com/c-api/group__c-api-basic-subscription-functions.html#ga94011b154c8836ff9a1d9018939f89cf)<br>[dxf_create_subscription_with_flags](https://docs.dxfeed.com/c-api/group__c-api-basic-subscription-functions.html#ga72d2af657437fb51a803daf55b4bbaf3)					|[dxf_subscription_t](https://docs.dxfeed.com/c-api/DXTypes_8h.html#a38b7d04b0d74a4bd070cf2e991b396fc)|Creates subscription to an event 					|[CommandLineSample](https://github.com/dxFeed/dxfeed-c-api/blob/master/samples/CommandLineSample/CommandLineSample.c#L791)<br>[CommandLineSample](https://github.com/dxFeed/dxfeed-c-api/blob/master/samples/CommandLineSample/CommandLineSample.c#L793)				|
|2|[dxf_create_snapshot](https://docs.dxfeed.com/c-api/group__c-api-snapshots.html#gaa3469159e760af9b1c58fa80b5de5630)<br>[dxf_create_candle_snapshot](https://docs.dxfeed.com/c-api/group__c-api-snapshots.html#gaf8a7439cdc0d88bc8f5ebaf170d9927b)<br>[dxf_create_order_snapshot](https://docs.dxfeed.com/c-api/group__c-api-snapshots.html#ga93344b5d1f8c059160e0d6e67f277c0f)|[dxf_snapshot_t](https://docs.dxfeed.com/c-api/DXTypes_8h.html#a93805d1b84e366f3654ac2f889edf74e)|Creates a snapshot subscription				|[SnapshotConsoleSample](https://github.com/dxFeed/dxfeed-c-api/blob/master/samples/SnapshotConsoleSample/SnapshotConsoleSample.c#L674)<br>[SnapshotConsoleSample](https://github.com/dxFeed/dxfeed-c-api/blob/master/samples/SnapshotConsoleSample/SnapshotConsoleSample.c#L657)<br>[SnapshotConsoleSample](https://github.com/dxFeed/dxfeed-c-api/blob/master/samples/SnapshotConsoleSample/SnapshotConsoleSample.c#L666)			|
|3|[dxf_create_price_level_book](https://docs.dxfeed.com/c-api/group__c-api-price-level-book.html#ga25c42992aec7ea7c9c4216b73a582db8) |[dxf_price_level_book_t](https://docs.dxfeed.com/c-api/DXTypes_8h.html#a0129dd405921e4752f1f1a4cd20ee6cb)|Creates the new price level book for the specified symbol and sources 		|[PriceLevelBookSample](https://github.com/dxFeed/dxfeed-c-api/blob/master/samples/PriceLevelBookSample/PriceLevelBookSample.c#L334)|
|4|[dxf_create_regional_book](https://docs.dxfeed.com/c-api/group__c-api-regional-book.html#gad89f6e43b552aa8a5f5a1257d4e9072f)|[dxf_regional_book_t](https://docs.dxfeed.com/c-api/DXTypes_8h.html#ad8c847bf57cdd7d78c35500352ac5e77)|Creates a regional price level book 		|[RegionalBookSample](https://github.com/dxFeed/dxfeed-c-api/blob/master/samples/RegionalBookSample/RegionalBookSample.c#L321)|


---

#### Listener attachment

|#		|Method				|Handle			|Callback listener 		|
|:-----:|:------------------|:--------------------------|:------------------|
|1|[dxf_attach_event_listener](https://docs.dxfeed.com/c-api/group__c-api-event-listener-functions.html#gab1fbf4332e2d48d7be81e1360f24d3ce)					|[dxf_subscription_t](https://docs.dxfeed.com/c-api/DXTypes_8h.html#a38b7d04b0d74a4bd070cf2e991b396fc)|[dxf_event_listener_t](https://docs.dxfeed.com/c-api/group__event-data-structures-event-subscription-stuff.html#gac8bcb70cd4c8857f286f4be65e9522c6)					|[CommandLineSample](https://github.com/dxFeed/dxfeed-c-api/blob/master/samples/CommandLineSample/CommandLineSample.c#L805)				|
|2|[dxf_attach_snapshot_listener](https://docs.dxfeed.com/c-api/group__c-api-snapshots.html#gaefbb7eadd35487a1e076dd7d3e20cd1e)|[dxf_snapshot_t](https://docs.dxfeed.com/c-api/DXTypes_8h.html#a93805d1b84e366f3654ac2f889edf74e)|[dxf_snapshot_listener_t](https://docs.dxfeed.com/c-api/group__c-api-snapshots.html#ga6a855f3cc13129a3a7df7669e504d7de)				|[SnapshotConsoleSample](https://github.com/dxFeed/dxfeed-c-api/blob/master/samples/SnapshotConsoleSample/SnapshotConsoleSample.c#L685)		|
|3|[dxf_attach_price_level_book_listener](https://docs.dxfeed.com/c-api/group__c-api-price-level-book.html#ga7b9909bae236e8638e79f3b054d49278) |[dxf_price_level_book_t](https://docs.dxfeed.com/c-api/DXTypes_8h.html#a0129dd405921e4752f1f1a4cd20ee6cb)|[dxf_price_level_book_listener_t](https://docs.dxfeed.com/c-api/group__c-api-price-level-book.html#ga0bb93d79baa953f829ff9b4d2791c87b) 		|[PriceLevelBookSample](https://github.com/dxFeed/dxfeed-c-api/blob/master/samples/PriceLevelBookSample/PriceLevelBookSample.c#L344)|
|4|[dxf_attach_regional_book_listener](https://docs.dxfeed.com/c-api/group__c-api-regional-book.html#ga968bb73963a03615f323d93bd40adc4c)<br>[dxf_attach_regional_book_listener_v2](https://docs.dxfeed.com/c-api/group__c-api-regional-book.html#ga46bb7bc2ebb5bbbbbd8198cf0eed4af1) |[dxf_regional_book_t](https://docs.dxfeed.com/c-api/DXTypes_8h.html#ad8c847bf57cdd7d78c35500352ac5e77)|[dxf_price_level_book_listener_t](https://docs.dxfeed.com/c-api/group__c-api-price-level-book.html#ga0bb93d79baa953f829ff9b4d2791c87b)<br>[dxf_regional_quote_listener_t](https://docs.dxfeed.com/c-api/group__c-api-regional-book.html#gab127c35e5f1dba24b73a66138b8d2577) 		|[RegionalBookSample](https://github.com/dxFeed/dxfeed-c-api/blob/master/samples/RegionalBookSample/RegionalBookSample.c#L331)<br>[RegionalBookSample](https://github.com/dxFeed/dxfeed-c-api/blob/master/samples/RegionalBookSample/RegionalBookSample.c#L339)|

---

#### Order sources

Order source in most cases identifies source of **`Order`** and **`SpreadOrder`** events. You can set Order source in subscription creation method (e.g. **`order_source_ptr`** in **[dxf_create_order_snapshot()](https://github.com/dxFeed/dxfeed-c-api/blob/master/samples/SnapshotConsoleSample/SnapshotConsoleSample.c#L666)**) or by **[dxf_set_order_source()](https://docs.dxfeed.com/c-api/group__c-api-orders.html#gaf83819db83afb12e6691cd73be91b2ea)**. Several supported sources are listed below. Please find the full list **[here](https://kb.dxfeed.com/en/data-model/qd-model-of-market-events.html#UUID-858ebdb1-0127-8577-162a-860e97bfe408_para-idm53255963764388)**.

*Aggregated*:

* AGGREGATE_ASK, AGGREGATE_BID - Ask, Bid side of an aggregate order book (futures depth and Nasdaq Level II)

  ---
 *Full order depth*:
   
  * BATE - Cboe EU BXE (BATE)
  * BYX - Cboe BYX
  * BZX - Cboe BZX
  * BXTR - Cboe EU SI (Systematic Internaliser)
  * BI20 - BIST Top20 Orders (Level2+)
  * CHIX - Cboe EU CXE (Chi-X)
  * CEUX - Cboe EU DXE
  * CFE - Cboe CFE
  * C2OX - Cboe C2
  * DEA - Cboe EDGA
  * DEX - Cboe EDGX
  * ERIS - ErisX
  * ESPD - Nasdaq E-speed
  * FAIR - FairX
  * GLBX - Globex
  * ICE - ICE Futures US/EU
  * IST - Borsa Istanbul
  * MEMX - Members Exchange
  * NTV - Nasdaq TotalView
  * NFX - Nasdaq NFX
  * SMFE - SmallEx
  * XNFI - Nasdaq NFI
  * XEUR - Eurex
 ---
*Price level book*:
 
  * glbx - CME Globex
  * iex - IEX
  * memx - Members Exchange
  * ntv - Nasdaq TotalView
  * smfe - SmallEx
  * xeur - Eurex
    

| :information_source: CODE SAMPLE: take a look at `order_sources_ptr` usage in  [PriceLevelBookSample](https://github.com/dxFeed/dxfeed-c-api/blob/master/samples/PriceLevelBookSample/PriceLevelBookSample.c#L334)|
| --- |

|:white_check_mark: READ MORE: [Order sources](https://kb.dxfeed.com/en/data-model/qd-model-of-market-events.html#UUID-858ebdb1-0127-8577-162a-860e97bfe408_para-idm53255963764388)|
| --- |
   
---

## Usage

#### Create connection

```cpp
//connection handler declaration
dxf_connection_t connection;
//creating connection result declaration
ERRORCODE connection_result;
//creating connection
connection_result = dxf_create_connection(dxfeed_host, on_reader_thread_terminate, NULL, NULL, NULL, NULL, &connection);
```

---

#### Create subscription

```cpp
//subscription handler declaration
dxf_subscription_t subscription;
//creating subscription result declaration
ERRORCODE subscription_result;
//creating subscription handler
subscription_result = dxf_create_subscription(connection, event_type, &subscription);
```

---

#### Setting up contract type

```cpp
//creating subscription handler with Ticker contract
subscription_result = dxf_create_subscription_with_flags(connection, event_type, dx_esf_force_ticker, &subscription);
```

---

#### Setting up symbol

```cpp
//adding array of symbols
dxf_add_symbols(subscription, (dxf_const_string_t*)symbols, symbol_count)
```

---

#### Setting up Order source

```cpp
//setting Nasdaq TotalView FOD source
dxf_set_order_source(subscription, "NTV")
```

---

#### Quote subscription

```cpp
#include <stdio.h>
#include <time.h>
#include "DXFeed.h"
void print_timestamp(dxf_long_t timestamp) {
	wchar_t timefmt[80];
	struct tm* timeinfo;
	time_t tmpint = (time_t)(timestamp / 1000);
	timeinfo = localtime(&tmpint);
	wcsftime(timefmt, 80, L"%Y%m%d-%H%M%S", timeinfo);
	wprintf(L"%ls", timefmt);
}
void listener(int event_type, dxf_const_string_t symbol_name, const dxf_event_data_t* data, int data_count,
			  void* user_data) {
	wprintf(L"%Quote{symbol=%ls, ", symbol_name);
	dxf_quote_t* q = (dxf_quote_t*)data;
	wprintf(L"bidTime=");
	print_timestamp(q->bid_time);
	wprintf(L" bidExchangeCode=%c, bidPrice=%f, bidSize=%i, ", q->bid_exchange_code, q->bid_price, q->bid_size);
	wprintf(L"askTime=");
	print_timestamp(q->ask_time);
	wprintf(L" askExchangeCode=%c, askPrice=%f, askSize=%i, scope=%d}\n", q->ask_exchange_code, q->ask_price,
			q->ask_size, (int)q->scope);
}
int main(int argc, char* argv[]) {
	dxf_connection_t con;
	dxf_subscription_t sub;
	dxf_create_connection("demo.dxfeed.com:7300", NULL, NULL, NULL, NULL, NULL, &con);
	dxf_create_subscription(con, DXF_ET_QUOTE, &sub);
	dxf_const_string_t symbols2[] = {L"AAPL"};
	dxf_attach_event_listener(sub, listener, NULL);
	dxf_add_symbols(sub, symbols2, sizeof(symbols2) / sizeof(symbols2[0]));
	wprintf(L"Press enter to stop\n");
	getchar();
	dxf_close_subscription(sub);
	dxf_close_connection(con);
	return 0;
}
```



**Output:**

```
Quote{symbol=AAPL, bidTime=20210826-174935 bidExchangeCode=A, bidPrice=148.020000, bidSize=0, askTime=20210826-174936 askExchangeCode=A, askPrice=148.050000, askSize=0, scope=1}
Quote{symbol=AAPL, bidTime=20210826-174901 bidExchangeCode=B, bidPrice=147.710000, bidSize=0, askTime=20210826-174916 askExchangeCode=B, askPrice=148.280000, askSize=0, scope=1}
Quote{symbol=AAPL, bidTime=20210826-174901 bidExchangeCode=C, bidPrice=147.740000, bidSize=0, askTime=20210826-174901 askExchangeCode=C, askPrice=148.210000, askSize=0, scope=1}
Quote{symbol=AAPL, bidTime=20170704-054404 bidExchangeCode=D, bidPrice=646.490000, bidSize=0, askTime=20170704-054404 askExchangeCode=D, askPrice=653.370000, askSize=0, scope=1}
Quote{symbol=AAPL, bidTime=20210826-174930 bidExchangeCode=H, bidPrice=147.560000, bidSize=0, askTime=20210826-174933 askExchangeCode=H, askPrice=148.200000, askSize=0, scope=1}
Quote{symbol=AAPL, bidTime=19700101-030000 bidExchangeCode=I, bidPrice=nan, bidSize=0, askTime=19700101-030000 askExchangeCode=I, askPrice=nan, askSize=0, scope=1}
Quote{symbol=AAPL, bidTime=20210826-174935 bidExchangeCode=J, bidPrice=147.890000, bidSize=0, askTime=20210826-174935 askExchangeCode=J, askPrice=148.220000, askSize=0, scope=1}
```

---

## Samples

[https://github.com/dxFeed/dxfeed-c-api/tree/master/samples](https://github.com/dxFeed/dxfeed-c-api/tree/master/samples):

  * [CandleSample](https://github.com/dxFeed/dxfeed-c-api/tree/master/samples/CandleSample) - demonstrates how to subscribe to **`Candle`** event.
  * [CommandLineSample](https://github.com/dxFeed/dxfeed-c-api/tree/master/samples/CommandLineSample) - demonstrates how to subscribe to **`Quote`**, **`Trade`**, **`TradeETH`**, **`Order`**, **`SpreadOrder`**, **`Profile`**, **`Summary`**, **`TimeAndSale`**, **`Underlying`**, **`TheoPrice`**, **`Series`**, **`Greeks`**, **`Configuration`** events.
  * [IncSnapshotConsoleSample](https://github.com/dxFeed/dxfeed-c-api/tree/master/samples/IncSnapshotConsoleSample) - demonstrates how to subscribe to **`Order`**, **`SpreadOrder`**, **`TimeAndSale`**, **`Series`**, **`Greeks`** events with a snapshot and incremental updates.
  * [PriceLevelBookSample](https://github.com/dxFeed/dxfeed-c-api/tree/master/samples/PriceLevelBookSample) - demonstrates how to subscribe to a price level book.
  * [RegionalBookSample](https://github.com/dxFeed/dxfeed-c-api/tree/master/samples/RegionalBookSample) - demonstrates how to subscribe to regional price level book.
  * [SnapshotConsoleSample](https://github.com/dxFeed/dxfeed-c-api/tree/master/samples/SnapshotConsoleSample) - demonstrates how to subscribe to **`Order`**, **`SpreadOrder`**, **`Candle`**, **`TimeAndSale`**, **`Greeks`**, **`Series`** snapshots.
  * [FullOrderBookSample](https://github.com/dxFeed/dxfeed-c-api/tree/master/samples/FullOrderBookSample) - demonstrates how to subscribe to full order book (FOB) with NTV order source.
<!-- 2023.05.03 -->