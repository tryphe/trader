

**Prerequisite: Compiling Qt (unix/linux)**

Trader requires a Qt >= 5.10 build with the *core*, *network*, and *websockets* modules. To build:
 1. Open your source directory, ie. `cd ~/src`. If it doesn't exist, run `mkdir ~/src` first.
 3. Pull any currently maintained Qt source: `wget https://download.qt.io/archive/qt/5.13/5.13.0/single/qt-everywhere-src-5.13.0.tar.xz`
 4. Extract: `tar xf qt-everywhere-src-5.13.0.tar.xz`
 5. Go there: `cd qt-everywhere-src-5.13.0.tar.xz`
 6. Install dependencies: `sudo apt-get build-dep qt5-default`
 7. Configure Qt (and skip some things to improve compile time): 
`./configure -prefix /home/username/localqt/Qt-5.13.0-minimal/ -opensource -confirm-license -shared -release -nomake examples -nomake tests -skip qt3d -skip qtactiveqt -skip qtandroidextras -skip qtcanvas3d -skip qtcharts -skip qtdatavis3d -skip qtgamepad -skip qtgraphicaleffects -skip qtlocation -skip qtmultimedia -skip qtpurchasing -skip qtquickcontrols -skip qtquickcontrols2 -skip qtscript -skip qtscxml -skip qtsensors -skip qtserialbus -skip qtserialport -skip qtwebengine -skip qtwebview -skip qttools`
 8. If there are no errors, now you can run make: `make -j` (if low on RAM or single CPU, you can skip the `-j`)
 9. If there are no errors, run `make install`
 11. Now try and run `qmake -v` from your installation path: 
	 `/home/username/localqt/Qt-5.13.0-minimal/bin/qmake -v`
	You should get:
~~~
QMake version 3.1
Using Qt version 5.13.0 in /home/username/localqt/Qt-5.13.0-release/lib
~~~

**Compiling trader(unix/linux)**
 1. Open your source directory, ie. `cd ~/src` 
 2. Clone this repo: `git clone https://github.com/tryphe/trader`
 3. Go there: `cd trader`
 4. Hardcode your keys into `daemon/keydefs.h` (choose only one):
	- Run `python generate_keys.py`
	- or: Copy the example keydefs file: `cp daemon/keydefs.h.example daemon/keydefs.h` then paste your keys in with your favorite editor, or:
	- or (*non-hardcoded keys*): Create `keydefs.h` using the example file above, but leave the keys blank or as-is during compile, then use the runtime CLI to enter your keys into the bot: `trader-cli Poloniex setkeyandsecret <key> <secret>` (***be careful, this will leak your keys into the bash history***)
 5. Now that `daemon/keydefs.h` exists, you can build the project from the project's root directory:
 6. Run qmake: `/home/username/localqt/Qt-5.13.0-minimal/bin/qmake`
 7. Build your binaries:
	- Run `./build-all.sh`. This will build all exchange targets and the CLI using `make -j`.
	- Modify `daemon/build-config.h` with your build preferences and build using `make -j` or what you like.

**Running the daemon**
- Daemonize traderd so it stays running: `setsid ./traderd-poloniex`

**Tailing the logs**
- Running the daemons and relying terminal output is suboptimal if the terminal closes. It's enabled by default, but can be disabled in `daemon/build-config.h`. All output is also routed to the logfiles. There's a color log, and a noncolor log. To tail:
`tail -f --lines=200 ~/.config/pt/log.<press-tab>_color.txt`. Note: `~/.config/pt` for Poloniex, `~/.config/tt` for biTTrex, and `~/.config/bt` for Binance.

**Preparing to run trader-cli (unix/linux)**
 1. Edit `~/.bashrc` using your favorite text editor, adding these lines:
~~~
alias trader-cli-poloniex='~/src/trader/trader-cli Poloniex'
alias trader-cli-bittrex='~/src/trader/trader-cli Bittrex'
alias trader-cli-binance='~/src/trader/trader-cli Binance'
 ~~~
 2. Then run `source ~/.bashrc`
 3. Now you can access each daemon with a simple command, eg. Poloniex: `trader-cli-poloniex getbalances`



***FAQS***

**What is this madness?**

Trader is a bot that manages a set of ping pong positions while working within the confines of exchange limits. These limits can include a maximum number of orders, minimum price, maximum price, etc. Trader can also make one-time, maker or taker orders, with an optional timeout.

**What the bot is NOT**

The bot doesn't do TA or have any concept of strategy. It doesn't read charts or know fancy statistics. It also doesn't have persistence yet (coming soon).

Traderd is a rational bot with well defined targets, and is simply a tool of execution. Anything strategy related is intended to be running on top of this bot, in order to maintain the bot's order spread over time, rather than of being part of the bot directly. It pongs the pings and pings the pongs, and runs onetime orders, and should be very efficient in general.

**What about supporting xyz exchange?**

It's fairly easily to integrate the bot with any API, as long as it can read its current orders and get ticker prices. Currently, it would take roughly 1000 lines to add another exchange. If you know of a good exchange, let me know.

**What's a ping-pong position?**

A ping-pong position is simple. It's defined as a price variation with an order size: 
- Order size
- Buy price
- Sell price
	
Suppose you want to ping-pong between buying 0.1 BTC worth shitcoins at 10 satoshi, and selling it at 30 satoshis. You'd run this command:
- `trader-cli-poloniex setorder BTC_DOGE buy 0.00000010 0.00000030 0.1 active`
- `buy` is the initial state
- `0.00000010` is the buy price
- `0.00000030` is the sell price
- `0.1` is the order size in BTC
- `active` tells the bot to set the order now (as opposed to setting it to `ghost` which lets the bot decide if it should set it)

[todo] explain ghost positions, onetime taker/maker orders, onetime order timeouts, ping-pong divergence/convergence settings, general market settings, base/quote pair formatting, calculating total position equity, calculating risk/reward
