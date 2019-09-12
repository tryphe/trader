What is Trader?
---------------
Trader is a bot that manages a set of ping pong positions while working within the confines of exchange limits. These limits can include a maximum number of orders, minimum price, maximum price, etc. Trader can also make one-time, fill-or-kill, and maker/taker orders, and simulates these modes on exchanges without direct support.

License
---------------
Trader is released under the terms of the MIT license.

Features
--------
 - [x] Low resource usage. 20-30MB of RAM and 1 CPU are required. (pi works fine!)
 - [x] Runs headless.
 - [x] Supports a large number of build targets. See [Qt platform requirements](https://doc-snapshots.qt.io/qt5-5.13/gettingstarted.html#platform-requirements) for more details.
 - [x] Order engine is adaptable to any exchange.
 - [x] Runtime [sanity testing](https://github.com/tryphe/trader/blob/master/daemon/coinamount_test.cpp#L12) and [fairly extensive order execution simulation](https://github.com/tryphe/trader/blob/master/daemon/engine_test.cpp#L17) for consistency.
 - [x] No floating point instructions are used. Uses GMP multi-precision integers. [How can floats fail?](https://github.com/tryphe/trader/blob/master/daemon/coinamount_test.cpp#L12)
 - [x] Real-time slippage calculation which prevents local order collision and greatly reduces the frequency of exchange post-only mode collisions.
 - [x] Anti-mistake. Prevents taking a price 10% lower or higher from the current spread(also overridable), [and more](https://github.com/tryphe/trader/blob/master/daemon/engine.cpp#L87)!
 - [x] Because trades aren't made using a browser interface, coin theft by withdrawal through browser hijacking is impossible.
 - [x] Poloniex REST and WSS feed support.
 - [x] Bittrex REST support.
 - [x] Binance REST support.
 
 TODO
 ----
  - [ ] GUI (work in progress)
  - [ ] WSS notifications (work in progress)
  - [ ] Add more exchanges.
  - [ ] Extensible API and querying system. (Note: CLI input and log output are the only interface at the moment)
  - [ ] External stats tracker for stats persistence.
  - [ ] User settings file.

Dependencies: Compiling Qt and installing libgmp (unix/linux)
---------------------------------
Trader requires a Qt >= 5.10 build with the *core*, *network*, and *websockets* modules. To build:
 1. Install dependencies: 
 	- `sudo apt build-dep qt5-default`
	- `sudo apt install libgmp-dev`
 2. Open your source directory, ie. `cd ~/src`. If it doesn't exist, run `mkdir ~/src` first.
 3. Pull any currently maintained Qt source: `wget https://download.qt.io/archive/qt/5.13/5.13.0/single/qt-everywhere-src-5.13.0.tar.xz`
 4. Extract: `tar xf qt-everywhere-src-5.13.0.tar.xz`
 5. Go there: `cd qt-everywhere-src-5.13.0`
 6. Configure Qt and skip some things to improve compile time. Replace `username` with your user. Note: If you want to build for another machine, [click here to configure a static build](https://gist.githubusercontent.com/tryphe/e3d17209ff6d53d2ca3c5d192471e12e/raw/3e85caf5c2e0fc63f36b7e7772771dff58bc174e/configure.static):\
`./configure -prefix /home/username/localqt/Qt-5.13.0-minimal/ -opensource -confirm-license -shared -release -nomake examples -nomake tests -skip qt3d -skip qtactiveqt -skip qtandroidextras -skip qtcanvas3d -skip qtcharts -skip qtdatavis3d -skip qtgamepad -skip qtgraphicaleffects -skip qtlocation -skip qtmultimedia -skip qtpurchasing -skip qtquickcontrols -skip qtquickcontrols2 -skip qtscript -skip qtscxml -skip qtsensors -skip qtserialbus -skip qtserialport -skip qtwebengine -skip qtwebview -skip qttools`
 7. If there are no errors, now you can run make: `make -j` (if low on RAM or single CPU, you can skip the `-j`)
 8. If there are no errors, run `make install`
 9. Now try and run `qmake -v` from your installation path: 
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
 4. Hardcode your API keys into `daemon/keydefs.h` (choose only one):
	- Run `python generate_keys.py`
	- or: Copy the example keydefs file: `cp daemon/keydefs.h.example daemon/keydefs.h` then paste your keys in with your favorite editor.
	- or: (*non-hardcoded keys*): Create `keydefs.h` using the example file above, but leave the keys blank or as-is during compile, then use the runtime CLI to enter your keys into the bot: `trader-cli Poloniex setkeyandsecret <key> <secret>` (***be careful, this will leak your keys into the bash history***)
 5. Run qmake: `/home/username/localqt/Qt-5.13.0-minimal/bin/qmake`
 6. Compile (choose one):
 	- (scripted build) Run `./build-all.sh`. This will build the CLI and all exchange targets using `make -j`.
	- (manual build) First, run `./generate_certs.sh` to generate x509 certs and an auth token (for websockets). Then run `make -j` or similar to build the exchange selected in `daemon/build-config.h` and the CLI.

Running the daemon
------------------
1. Sessionize traderd so it stays running(one for each exchange): `setsid ./traderd-poloniex`
2. Edit `~/.bashrc` using your favorite text editor, adding these lines:
~~~
alias poloniex='~/src/trader/trader-cli Poloniex'
alias bittrex='~/src/trader/trader-cli Bittrex'
alias binance='~/src/trader/trader-cli Binance'
 ~~~
 3. Then run `source ~/.bashrc`

Now you can access each daemon with a simple command, eg. Poloniex: `poloniex getbalances`

**Tailing the logs (note: CLI output goes to the logs)**\
Running the daemons and relying terminal output is suboptimal if the terminal closes. It's enabled by default, but can be disabled in `daemon/build-config.h`. All output is also routed to the logfiles. There's a color log, and a noncolor log. To tail, run:
`tail -f --lines=200 ~/.config/pt/log.<press-tab>_color.txt`. Note: `~/.config/pt` for Poloniex, `~/.config/tt` for biTTrex, and `~/.config/bt` for Binance.

FAQS
----
**What the bot is NOT**

The bot doesn't read charts. It also doesn't have persistence yet (coming soon).

**What about supporting xyz exchange?**

It's fairly easily to integrate the bot with any API, as long as it can read its current orders and get ticker prices. Currently, it would take roughly 1000 lines to add another exchange. If you know of a good exchange, let me know.

**What's a ping-pong position?**

A ping-pong position is simple. It's defined as a price variation with an order size:
- Buy price
- Sell price
- Order size
- State - buy or sell
	
Suppose you want to ping-pong between buying 0.1 BTC worth of DOGE at 10 satoshi, and selling it at 30 satoshis. You'd run this command:\
`poloniex setorder BTC_DOGE buy 0.00000010 0.00000030 0.1 active` (or `BTC-DOGE` on Bittrex and `DOGEBTC` on Binance)
- `buy` is the initial state
- `0.00000010` is the buy price
- `0.00000030` is the sell price
- `0.1` is the order size in BTC
- `active` tells the bot to set the order now (as opposed to setting it to `ghost` which lets the bot decide if it should set it)

**Placing different types of orders**

One-time taker order, buy 0.025 BTC of DOGE at 30 satoshi:\
`poloniex setorder BTC_DOGE buy 0.00000030 0.00000000 0.025 onetime-taker`

One-time maker order, sell 0.025 BTC of DOGE at 100 satoshi (maker-limit order):\
`poloniex setorder BTC_DOGE sell 0.00000000 0.00000100 0.025 onetime`

Same as above, but cancel if 5 minutes elapses (delayed fill or kill order):\
`poloniex setorder BTC_DOGE sell 0.00000000 0.00000100 0.025 onetime-timeout5`

Ping-pong order, buy at 17 satoshis, sell at 18, size 0.011:\
`poloniex setorder BTC_OMG buy 0.00000017 0.00000018 0.011 active`

Same as above, but when the ping-pong order is filled once, set size to 0.001 (effectively buys 0.011 and ping-pongs 0.001 of it):\
`poloniex setorder BTC_OMG buy 0.00000017 0.00000018 0.011/0.001 active`

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

`setordermin <market> <n>` is the number of minimum orders to maintain on *each side* of the market spread (bid/ask). If `orderdc >1`, `min = max-min`.

`setordermax <market> <n>` is the maximum number of orders on each side of the spread. This effectively maintains the *total* active orders at `min+(max-min)`.

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

When botting long-term, you MUST keep at least 50 orders total. This lets us mitigate an erroneous "blank but valid" exchange order list response where the exchange tells us we have no orders, but actually do. Does this happen? Yes it does, on every exchange, although very rarely.


[todo] explain ghost positions, other stuff
