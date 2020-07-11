# candlestick-puller

Rudimentary candlestick puller, currently only supports Bittrex, but can be modified to support more exchanges.

Dumps the candlesticks in OHLC4 `(open+high+low+close)/4` format, space delimited text, prepended by the epoch timestamp of the first candle, and a marker `p`:

`p <epoch> <ohlc4 1> <ohlc4 2> <ohlc4 n>...`

#### Market format
The market format on Bittrex is backwards from most exchanges. Where `LTC-BTC` is for quantities of `LTC` priced in `BTC`, some other exchanges will format this as `BTC_LTC`. Arguments are taken in the former format, but the file name will be saved in the latter format.

#### Exchange rate limits
Bittrex allows 1 command per second. Pulling candles for many markets may take hours, but at least you won't get banned. In order to support multiple pullers running and be nice to the API, it only sends a request after it gets a response.

#### Usage

`./candlestick-puller <bittrex market> <date> [invert flag]`

Pull the `LTC-BTC` market:

`./candlestick-puller LTC-BTC M3d7y2014`

Where `M3d7y2014` is month 3, day 7, year 2014, the date `LTC` became tradeable.
This will pull the candles and save to the file `BITTREX.BTC_LTC.5` when finished.

If you don't know exactly where the data starts, you can specify `?` at the end of an earlier date, and it will scan until it finds populated data:

`./candlestick-puller LTC-BTC M2d1y2014?`

If you want to invert the price of a market, for example `BTC-USDT`, but you want the price in `BTC` and not `USDT`, you can invert it:

`./candlestick-puller BTC-USDT M12d12y2015 invert`
Will save to the file `BITTREX.BTC_USDT.5` when finished.

#### Candlestick intervals

Currently, only 5 minute candles are supported. Please fork the code if you would like to change it.
