Features
--------
 - [x] Supports a large number of build platforms
 - [x] Order engine is adaptable to any exchange
 - [x] Runtime sanity testing and order execution simulation for consistency
 - [x] No floating point math operations are used for any prices or amounts. They are only used to store ratios which are fed as strings into GMP. [how can floats fail?](https://github.com/tryphe/trader/blob/master/daemon/coinamount_test.cpp#L12)
 - [x] Real-time slippage calculation which prevents local order collision and greatly reduces the frequency of exchange post-only mode collisions.
 - [x] Poloniex REST and WSS feed support
 - [x] Bittrex REST support
 - [x] Binance REST support
 
 TODO
 ----
  - [ ] Extensible API and querying system (CLI input and log output are the only interface at the moment)
  - [ ] External stats tracker for stats persistence
  - [ ] User settings file

Dependencies: Compiling Qt and installing libgmp (unix/linux)
---------------------------------
Trader requires a Qt >= 5.10 build with the *core*, *network*, and *websockets* modules. To build:
 1. Open your source directory, ie. `cd ~/src`. If it doesn't exist, run `mkdir ~/src` first.
 3. Pull any currently maintained Qt source: `wget https://download.qt.io/archive/qt/5.13/5.13.0/single/qt-everywhere-src-5.13.0.tar.xz`
 4. Extract: `tar xf qt-everywhere-src-5.13.0.tar.xz`
 5. Go there: `cd qt-everywhere-src-5.13.0.tar.xz`
 6. Install dependencies: 
 	- `sudo apt build-dep qt5-default`
	- `sudo apt install libgmp-dev`
 7. Configure Qt and skip some things to improve compile time. Replace `username` with your user. Note: If you want to build for another machine, [click here to configure a static build](https://gist.githubusercontent.com/tryphe/e3d17209ff6d53d2ca3c5d192471e12e/raw/3e85caf5c2e0fc63f36b7e7772771dff58bc174e/configure.static):\
`./configure -prefix /home/username/localqt/Qt-5.13.0-minimal/ -opensource -confirm-license -shared -release -nomake examples -nomake tests -skip qt3d -skip qtactiveqt -skip qtandroidextras -skip qtcanvas3d -skip qtcharts -skip qtdatavis3d -skip qtgamepad -skip qtgraphicaleffects -skip qtlocation -skip qtmultimedia -skip qtpurchasing -skip qtquickcontrols -skip qtquickcontrols2 -skip qtscript -skip qtscxml -skip qtsensors -skip qtserialbus -skip qtserialport -skip qtwebengine -skip qtwebview -skip qttools`
 8. If there are no errors, now you can run make: `make -j` (if low on RAM or single CPU, you can skip the `-j`)
 9. If there are no errors, run `make install`
 11. Now try and run `qmake -v` from your installation path: 
	 `/home/username/localqt/Qt-5.13.0-minimal/bin/qmake -v`
	You should get:
~~~
QMake version 3.1
Using Qt version 5.13.0 in /home/username/localqt/Qt-5.13.0-minimal/lib
~~~

Compiling
---------
 1. Open your source directory, ie. `cd ~/src` 
 2. Clone this repo: `git clone https://github.com/tryphe/trader`
 3. Go there: `cd trader`
 4. Hardcode your keys into `daemon/keydefs.h` (choose only one):
	- Run `python generate_keys.py`
	- or: Copy the example keydefs file: `cp daemon/keydefs.h.example daemon/keydefs.h` then paste your keys in with your favorite editor, or:
	- or (*non-hardcoded keys*): Create `keydefs.h` using the example file above, but leave the keys blank or as-is during compile, then use the runtime CLI to enter your keys into the bot: `trader-cli Poloniex setkeyandsecret <key> <secret>` (***be careful, this will leak your keys into the bash history***)
 5. Run qmake: `/home/username/localqt/Qt-5.13.0-minimal/bin/qmake`
 6. Compile (choose one):
 	- (scripted build) Run `./build-all.sh`. This will build the CLI and all exchange targets using `make -j`.
	- (manual build) Run `make -j` or similar to build the CLI and the exchange selected in `daemon/build-config.h`.

Running the daemon
------------------
1. Sessionize traderd so it stays running: `setsid ./traderd-poloniex`
2. Edit `~/.bashrc` using your favorite text editor, adding these lines:
~~~
alias trader-poloniex='~/src/trader/trader-cli Poloniex'
alias trader-bittrex='~/src/trader/trader-cli Bittrex'
alias trader-binance='~/src/trader/trader-cli Binance'
 ~~~
 3. Then run `source ~/.bashrc`

Now you can access each daemon with a simple command, eg. Poloniex: `trader-poloniex getbalances`

**Tailing the logs (note: CLI output goes to the logs)**\
Running the daemons and relying terminal output is suboptimal if the terminal closes. It's enabled by default, but can be disabled in `daemon/build-config.h`. All output is also routed to the logfiles. There's a color log, and a noncolor log. To tail, run:
`tail -f --lines=200 ~/.config/pt/log.<press-tab>_color.txt`. Note: `~/.config/pt` for Poloniex, `~/.config/tt` for biTTrex, and `~/.config/bt` for Binance.

FAQS
----
**What is this madness?**

Trader is a bot that manages a set of ping pong positions while working within the confines of exchange limits. These limits can include a maximum number of orders, minimum price, maximum price, etc. Trader can also make one-time, maker or taker orders, with an optional timeout.

**What the bot is NOT**

The bot doesn't do TA or have any concept of strategy. It doesn't read charts or know fancy statistics. It also doesn't have persistence yet (coming soon).

Traderd is a rational bot with well defined targets, and is simply a tool of execution. Anything strategy related is intended to be running on top of this bot, in order to maintain the bot's order spread over time, rather than of being part of the bot directly. It pongs the pings and pings the pongs, and runs onetime orders, and should be very efficient in general.

**What about supporting xyz exchange?**

It's fairly easily to integrate the bot with any API, as long as it can read its current orders and get ticker prices. Currently, it would take roughly 1000 lines to add another exchange. If you know of a good exchange, let me know.

**What's a ping-pong position?**

A ping-pong position is simple. It's defined as a price variation with an order size:
- Buy price
- Sell price
- Order size
- Initial state (buy or sell)
	
Suppose you want to ping-pong between buying 0.1 BTC worth of DOGE at 10 satoshi, and selling it at 30 satoshis. You'd run this command:\
`trader-poloniex setorder BTC_DOGE buy 0.00000010 0.00000030 0.1 active`
- `buy` is the initial state
- `0.00000010` is the buy price
- `0.00000030` is the sell price
- `0.1` is the order size in BTC
- `active` tells the bot to set the order now (as opposed to setting it to `ghost` which lets the bot decide if it should set it)

**Placing different types of orders**

One-time taker order, buy 0.025 BTC of DOGE at 30 satoshi:\
`trader-poloniex setorder BTC_DOGE buy 0.00000030 0.00000000 0.025 onetime-taker`

One-time maker order, sell 0.025 BTC of DOGE at 100 satoshi (maker-limit order):\
`trader-poloniex setorder BTC_DOGE sell 0.00000000 0.00000100 0.025 onetime`

Same as above, but cancel if 5 minutes elapses (delayed fill or kill order):\
`trader-poloniex setorder BTC_DOGE sell 0.00000000 0.00000100 0.025 onetime-timeout5`

Ping-pong order, buy at 17 satoshis, sell at 18, size 0.011:\
`trader-poloniex setorder BTC_OMG buy 0.00000017 0.00000018 0.011 active`

Same as above, but when the ping-pong order is filled once, set size to 0.001 (effectively buys 0.011 and ping-pongs 0.001 of it):\
`trader-poloniex setorder BTC_OMG buy 0.00000017 0.00000018 0.011/0.001 active`

**Enough of that, give me a real ping-pong example!**\
Note: For exchange commands, the formatting is literally `command <required-arg> [optional-arg=default-value]`\
Note: This is purely for example and so the buy and sell prices are so far apart, they'd probably never fill.\
First, buy a tiny amount of both pairs you'd like to trade. We'll use `BTC` and `OMG`.\
Then, paste this into `~/.config/pt/in.txt`: (note: `/tt` for Bittrex, `/bt` for Binance).
```
setorderdc BTC_OMG 5
setorderdcnice BTC_OMG 8
setorderlandmarkstart BTC_OMG 9
setorderlandmarkthresh BTC_OMG 10
setordermin BTC_OMG 11
setordermax BTC_OMG 44
setmarketoffset BTC_OMG 0.004
setmarketsentiment BTC_OMG false
setorder BTC_OMG buy 0.00000001 0.00100000 0.00011 ghost
setorder BTC_OMG buy 0.00000002 0.00200000 0.00011 ghost
setorder BTC_OMG buy 0.00000003 0.00300000 0.00011 ghost
setorder BTC_OMG buy 0.00000004 0.00400000 0.00011 ghost
setorder BTC_OMG buy 0.00000005 0.00500000 0.00011 ghost
setorder BTC_OMG buy 0.00000006 0.00600000 0.00011 ghost
setorder BTC_OMG buy 0.00000007 0.00700000 0.00011 ghost
setorder BTC_OMG buy 0.00000008 0.00800000 0.00011 ghost
setorder BTC_OMG buy 0.00000009 0.00900000 0.00011 ghost
setorder BTC_OMG buy 0.00000010 0.01000000 0.00011 ghost
setorder BTC_OMG buy 0.00000011 0.01100000 0.00011 ghost
setorder BTC_OMG buy 0.00000012 0.01200000 0.00011 active
setorder BTC_OMG buy 0.00000013 0.01300000 0.00011 active
setorder BTC_OMG buy 0.00000014 0.01400000 0.00011 active
setorder BTC_OMG buy 0.00000015 0.01500000 0.00011 active
setorder BTC_OMG buy 0.00000016 0.01600000 0.00011 active
setorder BTC_OMG sell 0.00000017 0.01700000 0.00011 active
setorder BTC_OMG sell 0.00000018 0.01800000 0.00011 active
setorder BTC_OMG sell 0.00000019 0.01900000 0.00011 active
setorder BTC_OMG sell 0.00000020 0.02000000 0.00011 active
setorder BTC_OMG sell 0.00000021 0.02100000 0.00011 ghost
setorder BTC_OMG sell 0.00000022 0.02200000 0.00011 ghost
setorder BTC_OMG sell 0.00000023 0.02300000 0.00011 ghost
setorder BTC_OMG sell 0.00000024 0.02400000 0.00011 ghost
setorder BTC_OMG sell 0.00000025 0.02500000 0.00011 ghost
setorder BTC_OMG sell 0.00000026 0.02600000 0.00011 ghost
setorder BTC_OMG sell 0.00000027 0.02700000 0.00011 ghost
setorder BTC_OMG sell 0.00000028 0.02800000 0.00011 ghost
setorder BTC_OMG sell 0.00000029 0.02900000 0.00011 ghost
setorder BTC_OMG sell 0.00000030 0.03000000 0.00011 ghost
setorder BTC_OMG sell 0.00000031 0.03100000 0.00011 ghost
setorder BTC_OMG sell 0.00000032 0.03200000 0.00011 ghost
setorder BTC_OMG sell 0.00000033 0.03300000 0.00011 ghost
setorder BTC_OMG sell 0.00000034 0.03400000 0.00011 ghost
```

Because we are giving the bot an initial state rather than setting one-time orders, we need some settings:

`setorderdc <market> <n>` converges n orders that are far away from the spread into 1 order, and diverges them back into n orders when it becomes close enough to spread. It combines all of the quantities and prices using the total quantity and quantity-weighted price average. These are called landmark orders, denoted by `L` in the bot.

`setorderlandmarkstart/thresh <market> <n>` are distances that dictate if the bot should set/cancel a lowest/highest order as a landmark, and the tolerance to noise.

`setorderdcnice <market> <n>` adjusts the noise of converging/diverging orders. higher = nicer, lower = more API spam.

`setordermin <market> <n>` is the number of minimum orders to maintain on each side (bid/ask) of the market spread. If `orderdc >1`, `min = max-min`.

`setordermax <market> <n>` is the max stated above. This attempts to maintain the count of active orders at `min+(max-min)`.

`setmarketsentiment <market> <bool>` false = short quote currency. true = long quote currency. In this case, the quote currency is OMG, and since it's false, we will short it by the amount below.

`setmarketoffset <market> <real>` "balances out" the exchange amounts after making trades by offsetting the amount bought from the amount sold. For example, if the fee is 0.2%, the exchange will take this amount of the base currency (BTC), times two; one for the buy, one for the sell. To offset it, you'd run: `setmarketoffset BTC_OMG 0.004` aka `(0.2% *2)`.

**How do I see my orders?**

Sorted by price: `getorders <market>` or by index: `getordersbyindex <market>`

**How do I cancel my orders?**

Cancelling non-bot orders: `cancelall [market="all"]` (Note: disabled if you have bot orders set, because it would interfere with fills)\
Cancelling bot orders: `cancellocal [market="all"]` (Note: won't interfere with orders you've set on the exchange.)

If you call the command without any arguments, cancel all markets, otherwise just cancel that one market.

**Once my spread moves, how can I restart the bot or revamp my order list?**

`savemarket [market="all"]` will save the market state to the file `/index-<market-name>.txt`.\
If you want to feed it back into the bot, copy it into in.txt ie `cp index-all.txt in.txt`.

**That's crazy. What's the point of all of this?**

Now that you've tried ping-pong orders, you should realize that you can shift the ping-pong spread up to the total equity on each side of your spread, essentially going short/long on the future value of one side of the spread (for example, cost averaging between multiple markets, or some other function), without a taker position, by inverting a ping-pong order to its opposite side, and taking a temporary slippage position that doesn't interfere with the spread: 

`short <market> [tag=""]`\
`long <market> [tag=""]` \
(Note: or `shortindex`/`longindex` to associate by index and not by price)\
(Note: you can also tag certain shorts/longs with a string and get back the total later with `getshortlong <tag>`)

**Rules of Thumb**

When botting long-term, you MUST keep at least 50 orders total. This lets us mitigate an erroneous "blank but valid" exchange order list response where the exchange tells us we have no orders, but actually do. Does this happen? Yes it does, on every exchange, although very rarely (although WSS on Poloniex relieves us from polling the order list on there).


[todo] explain ghost positions, other stuff
