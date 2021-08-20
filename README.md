#  ![logo1][]![logo2][] dxFeed C API 


[logo1]: docs/img/dxfeed_logo.png
[logo2]: docs/img/cpp_logo.png


This package provides access to **[dxFeed](https://www.dxfeed.com/)** market data.

[![Release](https://img.shields.io/github/v/release/dxFeed/dxfeed-c-api)](https://github.com/dxFeed/dxfeed-c-api/releases/latest)
[![License](https://img.shields.io/badge/license-MPL--2.0-orange)](https://github.com/dxFeed/dxfeed-net-api/blob/master/LICENSE)
[![Downloads](https://img.shields.io/github/downloads/dxFeed/dxfeed-c-api/total)](https://github.com/dxFeed/dxfeed-c-api/releases/latest)


## Table of Сontents
- [Documentation](#documentation)
- [Installation](#installation)
  * [Windows](#windows)
  * [Linux](#linux)
  * [macOS](#macos)
- [Key features](#key-features)
  * [Event types](#event-types)
  * [Contracts](#contracts)
    * [Ticker](#ticker) 
    * [Stream](#stream) 
    * [History](#history)  
  * [Subscription types](#subscription-types)
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
Find useful information in self-service dxFeed Knowledge Base, or .NET API framework documentation:

- [dxFeed Knowledge Base](https://kb.dxfeed.com/index.html?lang=en)
  * [Getting started](https://kb.dxfeed.com/en/getting-started.html)
  * [Troubleshooting](https://kb.dxfeed.com/en/troubleshooting-guidelines.html)
  * [Market events](https://kb.dxfeed.com/en/data-model/dxfeed-api-market-events.html)
  * [Event delivery contracts](https://kb.dxfeed.com/en/data-model/model-of-event-publishing.html#event-delivery-contracts)
  * [dxFeed API event classes](https://kb.dxfeed.com/en/data-model/model-of-event-publishing.html#dxfeed-api-event-classes)
  * [Exchange codes](https://kb.dxfeed.com/en/data-model/exchange-codes.html)
  * [Order sources](https://kb.dxfeed.com/en/data-model/qd-model-of-market-events.html#UUID-858ebdb1-0127-8577-162a-860e97bfe408_para-idm53255963764388)
  * [Order book reconstruction](https://kb.dxfeed.com/en/data-model/dxfeed-order-book-reconstruction.html)
  * [Incremental updates](https://kb.dxfeed.com/en/data-services/real-time-data-services/-net-api-incremental-updates.html)
  * [IPF live updates](https://kb.dxfeed.com/en/data-services/real-time-data-services/ipf-live-updates.html#-net-api)
  * [Symbol guide](https://downloads.dxfeed.com/specifications/dxFeed-Symbol-Guide.pdf)
- [dxFeed .NET API framework Documentation](https://docs.dxfeed.com/net-api/index.html?_ga=2.192331050.1892162722.1627897412-849088511.1627377760)
  * [Order scopes](https://docs.dxfeed.com/net-api/namespacecom_1_1dxfeed_1_1api_1_1data.html#ac2ccba635376abd940e96dd7e2664470)
  * [Event flags](https://docs.dxfeed.com/net-api/namespacecom_1_1dxfeed_1_1api_1_1data.html#af7e07c19db7804bc4727483f0c59fe4d)
  * [Subscriptions](https://docs.dxfeed.com/net-api/classcom_1_1dxfeed_1_1native_1_1NativeConnection.html#ad34451234590a5727fea7284ff24f5b4)






## Installation

### Windows

System requirements: [Visual C++ Redistributable 2015](https://www.microsoft.com/en-us/download/details.aspx?id=52685), [.NET Framework 4.5.2+](https://www.microsoft.com/en-us/download/details.aspx?id=30653), [Visual Studio](https://visualstudio.microsoft.com/vs/).


1. Download the **[latest](https://github.com/dxFeed/dxfeed-net-api/releases/latest)** version of dxFeed .NET API (`dxfeed-net-api-x.x.x-windows.zip`).
2. Extract archive.
3. Copy `dxf_api.dll`, `dxf_native.dll`, `DXFeed.dll`, `DXFeed_64.dll`, `DXFeedd.dll`, `DXFeedd_64.dll`, `libcrypto-41.dll`, `libcrypto-41_64.dll`, `libssl-43.dll`, `libssl-43_64.dll`, `libtls-15.dll`, `libtls-15_64.dll` to `\lib` folder of your Project.
4. Open Visual Studio.
5. Open Solution Explorer 🠒 Project (`.csproj` file) 🠒 References 🠒 Add Reference 🠒 Browse 🠒 Add. Then choose `dxf_api.dll`, `dxf_native.dll` libraries to add them to your Project.
6. Open Solution Explorer 🠒 Project (`.csproj` file) 🠒 Add 🠒 Existing Item 🠒 Add As Link. Then choose `DXFeed.dll`, `DXFeed_64.dll`, `DXFeedd.dll`, `DXFeedd_64.dll`, `libcrypto-41.dll`, `libcrypto-41_64.dll`, `libssl-43.dll`, `libssl-43_64.dll`, `libtls-15.dll`, `libtls-15_64.dll` libraries to add them to your Project.
7. Add `using` directive in class (`.cs` file):
```csharp
using com.dxfeed.api;
using com.dxfeed.native;
```
8. Look through the **[usage](#usage)** section and code **[samples](#samples)**.

---

### Linux
*Coming soon*

---


### macOS

*Coming soon*


## Key features

#### Event types

| #		| EventType			| Short description																				|Examples of original feed events	|Interface	|
| :----:|:------------------|:----------------------------------------------------------------------------------------------|:------|:----------|
| 1		|[Trade](https://kb.dxfeed.com/en/data-model/qd-model-of-market-events.html#trade-19593)	 			|The price and size of the last trade during regular trading hours, an overall day volume and day turnover						|Trade (last sale), trade conditions change messages, volume setting events, index value 		|[IDxTrade](https://docs.dxfeed.com/net-api/interfacecom_1_1dxfeed_1_1api_1_1events_1_1IDxTrade.html)|
| 2		| [TradeETH](https://kb.dxfeed.com/en/data-model/qd-model-of-market-events.html#tradeeth-19593)			|The price and size of the last trade during extended trading hours, and the extended trading hours day volume and day turnover					|Trade (last sale), trade conditions change messages, volume setting events		|[IDxTradeETH](https://docs.dxfeed.com/net-api/interfacecom_1_1dxfeed_1_1api_1_1events_1_1IDxTradeETH.html)|
| 3		|[TimeAndSale](https://kb.dxfeed.com/en/data-model/qd-model-of-market-events.html#timeandsale-19593)		|The trade or other market event with price that provides information about trades in a continuous time slice (unlike Trade events, which are supposed to provide a snapshot of the current last trade)															|Trade, index value|[IDxTimeAndSale](https://docs.dxfeed.com/net-api/interfacecom_1_1dxfeed_1_1api_1_1events_1_1IDxTimeAndSale.html)|
| 4		|[Quote](https://kb.dxfeed.com/en/data-model/qd-model-of-market-events.html#quote-19593)				|The best bid and ask prices and other fields that may change with each quote. It represents the most recent information that is available about the best quote on the market																	|BBO Quote (bid/ask), regional Quote (bid/ask) |[IDxQuote](https://docs.dxfeed.com/net-api/interfacecom_1_1dxfeed_1_1api_1_1events_1_1IDxQuote.html)|
| 5		|[Order](https://kb.dxfeed.com/en/data-model/dxfeed-api-market-events.html#orders)				|Depending on the **`Scope`** flag it may be: composite BBO from the whole market **or** regional BBO from a particular exchange **or** aggregated information (e.g. *PLB - price level book*) **or** individual order (*FOD - full order depth*)																				|Regional Quote (bid/ask), depth (order books, price levels, market maker quotes), market depth|[IDxOrder](https://docs.dxfeed.com/net-api/interfacecom_1_1dxfeed_1_1api_1_1events_1_1IDxOrder.html)|
| 6		|[SpreadOrder](https://kb.dxfeed.com/en/data-model/dxfeed-api-market-events.html#spreadorder)		|Multi-leg order															|Regional Quote (bid/ask), depth (order books, price levels, market maker quotes), market depth|[IDxSpreadOrder](https://docs.dxfeed.com/net-api/interfacecom_1_1dxfeed_1_1api_1_1events_1_1IDxSpreadOrder.html)||
| 7		| Candle			|OHLCV candle																					|Charting aggregations|[IDxCandle](https://docs.dxfeed.com/net-api/interfacecom_1_1dxfeed_1_1api_1_1events_1_1IDxCandle.html)|
| 8		|[Profile](https://kb.dxfeed.com/en/data-model/qd-model-of-market-events.html#profile-19593)			|The most recent information that is available about the traded security on the market																		|Instrument definition, trading halt/resume messages|[IDxProfile](https://docs.dxfeed.com/net-api/interfacecom_1_1dxfeed_1_1api_1_1events_1_1IDxProfile.html)|
| 9		|[Summary](https://kb.dxfeed.com/en/data-model/qd-model-of-market-events.html#summary-19593)			|The most recent OHLC information about the trading session on the market														|OHLC setting events (trades, explicit hi/lo update messages, explicit summary messages)|[IDxSummary](https://docs.dxfeed.com/net-api/interfacecom_1_1dxfeed_1_1api_1_1events_1_1IDxSummary.html)|
| 10	|[Greeks](https://kb.dxfeed.com/en/data-model/dxfeed-api-market-events.html#greeks)			|Differential values that show how the price of an option depends on other market parameters		|Greeks and Black-Scholes implied volatility|[IDxGreeks](https://docs.dxfeed.com/net-api/interfacecom_1_1dxfeed_1_1api_1_1events_1_1IDxGreeks.html)|
| 11	|[TheoPrice](https://kb.dxfeed.com/en/data-model/dxfeed-api-market-events.html#theoprice)			|The theoretical option price					|Theoretical prices|[IDxTheoPrice](https://docs.dxfeed.com/net-api/interfacecom_1_1dxfeed_1_1api_1_1events_1_1IDxTheoPrice.html)|
| 12	|[Underlying](https://kb.dxfeed.com/en/data-model/dxfeed-api-market-events.html#underlying)		|Calculated values that are available for an option underlying symbol based on the option prices on the market																|VIX methodology implied volatility, P/C ratios|[IDxUnderlying](https://docs.dxfeed.com/net-api/interfacecom_1_1dxfeed_1_1api_1_1events_1_1IDxUnderlying.html)||
| 13	|[Series](https://kb.dxfeed.com/en/data-model/dxfeed-api-market-events.html#series)			|Properties of the underlying																	|VIX methodology implied volatility, P/C ratios|[IDxSeries](https://docs.dxfeed.com/net-api/interfacecom_1_1dxfeed_1_1api_1_1events_1_1IDxSeries.html)|


|:information_source: CODE SAMPLE:  take a look at `EventType` usage in  [dxf_events_sample](https://github.com/dxFeed/dxfeed-net-api/tree/master/samples/dxf_events_sample), [dxf_candle_sample](https://github.com/dxFeed/dxfeed-net-api/tree/master/samples/dxf_candle_sample)|
| --- |

|:white_check_mark: READ MORE: [Events to be published for different feed types, feed types and events matrix](https://kb.dxfeed.com/en/data-model/model-of-event-publishing.html#events-to-be-published-for-different-feed-types)|
| --- |

---
#### Contracts

There are three types of delivery contracts: **`Ticker`**, **`Stream`** and **`History`**. You can set up the contract type by **[EventSubscriptionFlag](https://docs.dxfeed.com/net-api/namespacecom_1_1dxfeed_1_1api_1_1data.html#a5e593e65b38494fc19218527ea9eb4ac)** as a parameter of **[NativeConnection](https://docs.dxfeed.com/net-api/classcom_1_1dxfeed_1_1native_1_1NativeConnection.html)**.
  
##### Ticker
The main task of this contract is to reliably deliver the latest value for an event (for example, for the last trade of the selected symbol). Queued older events could be conflated to conserve bandwidth and resources.
 
##### Stream
A stream contract guaranteedly delivers a stream of events without conflation.

##### History
History contract first delivers a snapshot (set of previous events) for the specified time range (subject to limitations). After the snapshot is transmitted, the history contract delivers a stream of events.

|:information_source: NOTE: if **[EventSubscriptionFlag](https://docs.dxfeed.com/net-api/namespacecom_1_1dxfeed_1_1api_1_1data.html#a5e593e65b38494fc19218527ea9eb4ac)** is not set, the default contract type will be enabled.|
| --- |

Default contracts for events:

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


|:information_source: CODE SAMPLE: take a look at `EventSubscriptionFlag` usage in [dxf_client](https://github.com/dxFeed/dxfeed-net-api/blob/master/dxf_client/Program.cs#L309)|
| --- |

|:white_check_mark: READ MORE: [Event delivery contracts](https://kb.dxfeed.com/en/data-model/model-of-event-publishing.html#event-delivery-contracts)|
| --- |

---

#### Subscription types


|#		|Subscription type			| Description			| Code sample	|
|:-----:|:------------------|:------------------|:----------|
|1|[CreateSubscription](https://docs.dxfeed.com/net-api/classcom_1_1dxfeed_1_1native_1_1NativeConnection.html#a3d6ec48184c5ca089cd1b8df2addbba0)					|Creates subscription to an event 					|[dxf_events_sample](https://github.com/dxFeed/dxfeed-net-api/tree/master/samples/dxf_events_sample)				|
|2|[CreateSnapshotSubscription](https://docs.dxfeed.com/net-api/classcom_1_1dxfeed_1_1native_1_1NativeConnection.html#a7daf40732ea72fd061139ce74fe0889c)			|Creates a snapshot subscription				|[dxf_snapshot_sample](https://github.com/dxFeed/dxfeed-net-api/tree/master/samples/dxf_snapshot_sample)			|
|3|[CreateIncOrderSnapshotSubscription](https://docs.dxfeed.com/net-api/classcom_1_1dxfeed_1_1native_1_1NativeConnection.html#ad34451234590a5727fea7284ff24f5b4) |Creates the new native subscription to Order snapshot with incremental updates 		|[dxf_inc_order_snapshot_sample](https://github.com/dxFeed/dxfeed-net-api/tree/master/samples/dxf_inc_order_snapshot_sample)|
|4|[CreatePriceLevelBook](https://docs.dxfeed.com/net-api/classcom_1_1dxfeed_1_1native_1_1NativeConnection.html#a836f311b5f0ea07dbeb40a49d34f7dfb) |Creates the new price level book (10 levels) for the specified symbol and sources 		|[dxf_price_level_book_sample](https://github.com/dxFeed/dxfeed-net-api/tree/master/samples/dxf_price_level_book_sample)|
|5|[CreateRegionalBook](https://docs.dxfeed.com/net-api/classcom_1_1dxfeed_1_1native_1_1NativeConnection.html#a9548fc1f0ad480a4e1259366d30afdeb) |Creates a regional price level book (10 levels) 		|[dxf_regional_book_sample](https://github.com/dxFeed/dxfeed-net-api/tree/master/samples/dxf_regional_book_sample)|


---

#### Order sources

Order source identifies source of **`IDxOrder`** and **`IDxSpreadOrder`** events. You can set Order source by **[SetSource()](https://docs.dxfeed.com/net-api/interfacecom_1_1dxfeed_1_1api_1_1IDxSubscription.html#a56e276e3e36cc2e2fcb42de8f3f0bc95)** method of **[IDxSubscription](https://docs.dxfeed.com/net-api/interfacecom_1_1dxfeed_1_1api_1_1IDxSubscription.html)**. Several supported sources are listed below. Please find the full list **[here](https://kb.dxfeed.com/en/data-model/qd-model-of-market-events.html#UUID-858ebdb1-0127-8577-162a-860e97bfe408_para-idm53255963764388)**.

*Aggregated*:

* [AGGREGATE_ASK](https://docs.dxfeed.com/net-api/classcom_1_1dxfeed_1_1api_1_1events_1_1OrderSource.html#ae4b3536e7c787718c3017175f0a3eb85), [AGGREGATE_BID](https://docs.dxfeed.com/net-api/classcom_1_1dxfeed_1_1api_1_1events_1_1OrderSource.html#a6f740564e75018a1dea27d33ba1850ad) - Ask, Bid side of an aggregate order book (futures depth and Nasdaq Level II).

  ---
 *Full order depth*:
   
  * [BATE](https://docs.dxfeed.com/net-api/classcom_1_1dxfeed_1_1api_1_1events_1_1OrderSource.html#a23a77d857f81a924ec39e95c9ad47b64) - Cboe EU BXE (BATE).
  * [BYX](https://docs.dxfeed.com/net-api/classcom_1_1dxfeed_1_1api_1_1events_1_1OrderSource.html#ab9c99c1863c2dab4d75be6f2cafbf65f) - Cboe BYX.
  * [BZX](https://docs.dxfeed.com/net-api/classcom_1_1dxfeed_1_1api_1_1events_1_1OrderSource.html#a45a3a3743efd52a4c876297eb4c93530) - Cboe BZX.
  * [BXTR](https://docs.dxfeed.com/net-api/classcom_1_1dxfeed_1_1api_1_1events_1_1OrderSource.html#a000409309a79719d6c0b234bfa0cb216) - Cboe EU SI (Systematic Internaliser).
  * [BI20](https://docs.dxfeed.com/net-api/classcom_1_1dxfeed_1_1api_1_1events_1_1OrderSource.html#a401d886cdfc55c8f45d0a2ebe01c19df) - BIST Top20 Orders (Level2+).
 * [CHIX](https://docs.dxfeed.com/net-api/classcom_1_1dxfeed_1_1api_1_1events_1_1OrderSource.html#aab1c4c58295eebc0d120fceaab012f31) - Cboe EU CXE (Chi-X).
 * [CEUX](https://docs.dxfeed.com/net-api/classcom_1_1dxfeed_1_1api_1_1events_1_1OrderSource.html#aebd2fc2105aabd678cd7ecfb10f4a27d) - Cboe EU DXE.
  * [CFE](https://docs.dxfeed.com/net-api/classcom_1_1dxfeed_1_1api_1_1events_1_1OrderSource.html#aa9b0a589becd027f99abd7d8b41903ec) - Cboe CFE.
  * [C2OX](https://docs.dxfeed.com/net-api/classcom_1_1dxfeed_1_1api_1_1events_1_1OrderSource.html#a624d1551b9707dfc066fca87799b644e) - Cboe C2.
  * [DEA](https://docs.dxfeed.com/net-api/classcom_1_1dxfeed_1_1api_1_1events_1_1OrderSource.html#a4ac50b91676018000d78ef92f74fcd80) - Cboe EDGA.
  * [DEX](https://docs.dxfeed.com/net-api/classcom_1_1dxfeed_1_1api_1_1events_1_1OrderSource.html#a4851ca2f77f277f2bab72ef70a85f8c5) - Cboe EDGX.
  * [ERIS](https://docs.dxfeed.com/net-api/classcom_1_1dxfeed_1_1api_1_1events_1_1OrderSource.html#ab7291c6871603b715c3eb3a8349d1d52) - ErisX.
  * [ESPD](https://docs.dxfeed.com/net-api/classcom_1_1dxfeed_1_1api_1_1events_1_1OrderSource.html#a77c2576868b1d961683b8b07d544bc81) - Nasdaq E-speed.
  * [FAIR](https://docs.dxfeed.com/net-api/classcom_1_1dxfeed_1_1api_1_1events_1_1OrderSource.html#a2f381c0b42adba5a684f71657a4808ec) - FairX.
  * [GLBX](https://docs.dxfeed.com/net-api/classcom_1_1dxfeed_1_1api_1_1events_1_1OrderSource.html#aa59fc116ea5f2da46db2154d9d0188db) - Globex.
  * [ICE](https://docs.dxfeed.com/net-api/classcom_1_1dxfeed_1_1api_1_1events_1_1OrderSource.html#ae5831b47c22cb18cc7df8154ab2b93aa) - ICE Futures US/EU.
   * [IST](https://docs.dxfeed.com/net-api/classcom_1_1dxfeed_1_1api_1_1events_1_1OrderSource.html#aa6b876caba3ec65fae6864e63a68054b) - Borsa Istanbul.
   * [MEMX](https://docs.dxfeed.com/net-api/classcom_1_1dxfeed_1_1api_1_1events_1_1OrderSource.html#a30869000432bedf174b1fba4cbfa619e) - Members Exchange.
  * [NTV](https://docs.dxfeed.com/net-api/classcom_1_1dxfeed_1_1api_1_1events_1_1OrderSource.html#a5f01007b4cffede27b46cfc36a972755) - Nasdaq TotalView.
  * [NFX](https://docs.dxfeed.com/net-api/classcom_1_1dxfeed_1_1api_1_1events_1_1OrderSource.html#a7afd6f000a026f553c067fad6bc1ec26) - Nasdaq NFX.
* [SMFE](https://docs.dxfeed.com/net-api/classcom_1_1dxfeed_1_1api_1_1events_1_1OrderSource.html#aa4cf474b8549ab33d534ce01f3fa6fa0) - SmallEx.
 * [XNFI](https://docs.dxfeed.com/net-api/classcom_1_1dxfeed_1_1api_1_1events_1_1OrderSource.html#ae375ce1b060b648eae6b2ea4e187114f) - Nasdaq NFI.
 * [XEUR](https://docs.dxfeed.com/net-api/classcom_1_1dxfeed_1_1api_1_1events_1_1OrderSource.html#ac906af21e36f9ff86433c0b6b5959101) - Eurex.
 ---
*Price level book*:
 
  * [glbx](https://docs.dxfeed.com/net-api/classcom_1_1dxfeed_1_1api_1_1events_1_1OrderSource.html#aec52a8353354ce8ffe842796b254eaab) - CME Globex.
  * [iex](https://docs.dxfeed.com/net-api/classcom_1_1dxfeed_1_1api_1_1events_1_1OrderSource.html#a595576f4a4e0e03f3c72d05c8f20dfb2) - IEX.
  * [memx](https://docs.dxfeed.com/net-api/classcom_1_1dxfeed_1_1api_1_1events_1_1OrderSource.html#ab156ad040eb3abd3724537b557794f59) - Members Exchange.
  * [ntv](https://docs.dxfeed.com/net-api/classcom_1_1dxfeed_1_1api_1_1events_1_1OrderSource.html#aa595f38f874a185d97b78d7f4fc51b78) - Nasdaq TotalView.
   * [smfe](https://docs.dxfeed.com/net-api/classcom_1_1dxfeed_1_1api_1_1events_1_1OrderSource.html#a41e87bde8b836f1519b623079ba7767b) - SmallEx.
  * [xeur](https://docs.dxfeed.com/net-api/classcom_1_1dxfeed_1_1api_1_1events_1_1OrderSource.html#ae5b162c893b6bc58695cdd57e2259902) - Eurex.
    

| :information_source: CODE SAMPLE: take a look at `OrderSource` usage in  [dxf_price_level_book_sample](https://github.com/dxFeed/dxfeed-net-api/tree/master/samples/dxf_price_level_book_sample)|
| --- |

|:white_check_mark: READ MORE: [Order sources](https://kb.dxfeed.com/en/data-model/qd-model-of-market-events.html#UUID-858ebdb1-0127-8577-162a-860e97bfe408_para-idm53255963764388)|
| --- |
   
---

## Usage

#### Create connection

```csharp
//creating connection handler
var connection = new NativeConnection("demo.dxfeed.com:7300", _ => { });
```

---

#### Create subscription

```csharp
//creating subscription handler
var subscription = connection.CreateSubscription(EventType.Quote, new EventPrinter());
```

---

#### Setting up contract type

```csharp
//creating subscription handler with Ticker contract
var subscription = connection.CreateSubscription(EventType.Quote, EventSubscriptionFlag.ForceTicker, new EventPrinter());
```

---

#### Setting up symbol

```csharp
//adding single symbol
subscription.AddSymbols("AAPL");
//adding array of symbols
subscription.AddSymbols(new string[] { "AAPL", "MSFT" });
//adding all available symbols
subscription.AddSymbols("*");
```

---

#### Setting up Order source

```csharp
//setting Nasdaq TotalView FOD source
 subscription.SetSource("NTV");
```

---

#### Quote subscription

```csharp
//creating connection handler
using (var connection = new NativeConnection("demo.dxfeed.com:7300", DisconnectHandler))
           {
           //creating subscription handler,
           //passing object of type 'EventPrinter' as an argument 
           //to invoke callback method when data received 
               using (var subscription = connection.CreateSubscription(EventType.Quote, new EventPrinter()))
               {
               	   //adding subscription to 'AAPL' symbol
                   subscription.AddSymbols("AAPL");
                   Console.WriteLine("Press enter to stop");
                   Console.ReadLine();
               }
           }
```

```csharp
//implementation of 'DisconnectHandler' method which passed as an argument
//in 'NativeConnection' constructor
private static void DisconnectHandler(IDxConnection _) => 
	Console.WriteLine("Disconnected");
```

```csharp
//implementation of class 'EventPrinter' which object passed as an argument
//in 'CreateSubscription' method
public class EventPrinter: IDxFeedListener
{
	//implementation of callback method 'OnQuote' of 'IDxFeedListener' interface
        public void OnQuote<TB, TE>(TB buf)
            where TB : IDxEventBuf<TE>
            where TE : IDxQuote {
            foreach (var q in buf)
                Console.WriteLine($"{buf.Symbol} {q}");
        }
}
```

**Output:**

```
AAPL Quote: {AAPL, AskExchangeCode: 'U', Ask: 1@146.8, AskTime: 2021-07-29T12:42:21.0000000Z, BidExchangeCode: 'U', Bid: 1@144.5, BidTime: 2021-07-29T12:44:06.0000000Z, Scope: Regional}
AAPL Quote: {AAPL, AskExchangeCode: 'V', Ask: 0@144.94, AskTime: 2021-07-29T12:46:47.0000000Z, BidExchangeCode: 'V', Bid: 0@144.92, BidTime: 2021-07-29T12:47:17.0000000Z, Scope: Regional}
AAPL Quote: {AAPL, AskExchangeCode: 'W', Ask: 0@648.76, AskTime: 2017-07-04T02:44:04.0000000Z, BidExchangeCode: 'W', Bid: 0@641.61, BidTime: 2017-07-04T02:44:04.0000000Z, Scope: Regional}
AAPL Quote: {AAPL, AskExchangeCode: 'X', Ask: 4@148.5, AskTime: 2021-07-29T12:46:19.0000000Z, BidExchangeCode: 'X', Bid: 0@144.73, BidTime: 2021-07-29T12:42:07.0000000Z, Scope: Regional}
AAPL Quote: {AAPL, AskExchangeCode: 'Y', Ask: 0@144.88, AskTime: 2021-07-29T12:44:50.0000000Z, BidExchangeCode: 'Y', Bid: 20@140.35, BidTime: 2021-07-29T12:42:06.0000000Z, Scope: Regional}
AAPL Quote: {AAPL, AskExchangeCode: 'Z', Ask: 1@145.69, AskTime: 2021-07-29T12:46:51.0000000Z, BidExchangeCode: 'Z', Bid: 1@144.7, BidTime: 2021-07-29T12:47:28.0000000Z, Scope: Regional}
AAPL Quote: {AAPL, AskExchangeCode: 'K', Ask: 4@144.96, AskTime: 2021-07-29T12:47:28.0000000Z, BidExchangeCode: 'K', Bid: 2@144.93, BidTime: 2021-07-29T12:47:29.0000000Z, Scope: Composite}
```

---

## Samples

[https://github.com/dxFeed/dxfeed-net-api/tree/master/samples](https://github.com/dxFeed/dxfeed-net-api/tree/master/samples):

  * [dxf_candle_sample](https://github.com/dxFeed/dxfeed-net-api/tree/master/samples/dxf_candle_sample) - demonstrates how to subscribe to **`Candle`** event.
  * [dxf_events_sample](https://github.com/dxFeed/dxfeed-net-api/tree/master/samples/dxf_events_sample) - demonstrates how to subscribe to **`Quote`**, **`Trade`**, **`TradeETH`**, **`Order`**, **`SpreadOrder`**, **`Profile`**, **`Summary`**, **`TimeAndSale`**, **`Underlying`**, **`TheoPrice`**, **`Series`**, **`Greeks`**, **`Configuration`** events.
  * [dxf_inc_order_snapshot_sample](https://github.com/dxFeed/dxfeed-net-api/tree/master/samples/dxf_inc_order_snapshot_sample) - demonstrates how to subscribe to order snapshot with incremental updates.
  * [dxf_instrument_profile_live_sample](https://github.com/dxFeed/dxfeed-net-api/tree/master/samples/dxf_instrument_profile_live_sample) - demonstrates how to subscribe to IPF live updates.
  * [dxf_instrument_profile_sample](https://github.com/dxFeed/dxfeed-net-api/tree/master/samples/dxf_instrument_profile_sample) - demonstrates how to receive IPF data from URL or file.
  * [dxf_ipf_connect_sample](https://github.com/dxFeed/dxfeed-net-api/tree/master/samples/dxf_ipf_connect_sample) - demonstrates how to subscribe to events by symbols from downloaded IPF (from URL or file).
  * [dxf_price_level_book_sample](https://github.com/dxFeed/dxfeed-net-api/tree/master/samples/dxf_price_level_book_sample) - demonstrates how to subscribe to a price level book.
  * [dxf_read_write_raw_data_sample](https://github.com/dxFeed/dxfeed-net-api/tree/master/samples/dxf_read_write_raw_data_sample) - demonstrates how to save/read incoming binary traffic to/from file.
  * [dxf_regional_book_sample](https://github.com/dxFeed/dxfeed-net-api/tree/master/samples/dxf_regional_book_sample) - demonstrates how to subscribe to regional price level book.
  * [dxf_simple_data_retrieving_sample](https://github.com/dxFeed/dxfeed-net-api/tree/master/samples/dxf_simple_data_retrieving_sample) - demonstrates how to receive snapshots of **`TimeAndSale`**, **`Candle`**, **`Series`**, **`Greeks`** events on a given time interval without subscription.
  * [dxf_snapshot_sample](https://github.com/dxFeed/dxfeed-net-api/tree/master/samples/dxf_snapshot_sample) - demonstrates how to subscribe to **`Order`**, **`SpreadOrder`**, **`Candle`**, **`TimeAndSale`**, **`Greeks`**, **`Series`** snapshots.
