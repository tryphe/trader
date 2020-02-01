What is Trader?
---------------
Trader is a bot that makes manual orders for the user, and/or makes automatic ping-pong orders. Trader can execute one-time, fill-or-kill, and maker/taker orders, and simulates these modes on exchanges without direct support. Trader also respects exchange limits like maximum number of orders, minimum price, maximum price, minimum lot size, price ticksize, etc.

License
---------------
Trader is released under the terms of the MIT license.

Features
--------
 - [x] Low resource usage. 5MB static binary, 15-20MB ram usage.
 - [x] Runs headless.
 - [x] Supports a large number of build targets. See [Qt platform requirements](https://doc-snapshots.qt.io/qt5-5.13/gettingstarted.html#platform-requirements) for more details.
 - [x] Order engine is adaptable to any exchange.
 - [x] Runtime [sanity testing](https://github.com/tryphe/trader/blob/master/daemon/coinamount_test.cpp#L12) and [fairly extensive order execution simulation](https://github.com/tryphe/trader/blob/master/daemon/engine_test.cpp#L17) for consistency.
 - [x] Uses GMP multi-precision integers. No [floating-point](https://en.wikipedia.org/wiki/Floating_point) instructions are used. [How can floats fail?](https://github.com/tryphe/trader/blob/master/daemon/coinamount_test.cpp#L12)
 - [x] Real-time slippage calculation which prevents local order collision and greatly reduces the frequency of exchange post-only mode collisions.
 - [x] Anti-mistake. Prevents taking a price 10% lower or higher from the current spread(also overridable), [and more](https://github.com/tryphe/trader/blob/master/daemon/engine.cpp#L87)!
 - [x] ~Ping-pong using fixed, user-generated positions. Also able to combine positions.~ (recently deprecated and broken)
 - [x] Automatic ping-ping (wip)
 - [x] Because trades aren't made using a browser interface, coin theft by withdrawal through browser hijacking is impossible.

Exchanges
---------
 - [x] Bittrex REST support. (deprecated)
 - [x] Binance REST support. (deprecated, partially broken)
 - [x] Poloniex REST and WSS feed support (deprecated)
 - [x] Waves support

TODO
 ----
  - [ ] Stats tracker for persistence. (wip)
  - [ ] Add more exchanges.
  - [ ] GUI (abandoned)
  - [ ] WSS notifications (abandoned)

Dependencies: Compiling Qt and installing libgmp (unix/linux)
---------------------------------
Trader requires Qt 5.10 or later, built with the *core*, *network*, and *websockets* modules. To build:
 1. Install dependencies:
        - `sudo apt build-dep qt5-default`
        - `sudo apt install libgmp-dev`
 2. Open your source directory, ie. `cd ~/src`. If it doesn't exist, run `mkdir ~/src` first.
 3. Pull any currently maintained Qt source: `wget https://download.qt.io/archive/qt/5.13/5.13.1/single/qt-everywhere-src-5.13.1.tar.xz`
 4. Extract: `tar xf qt-everywhere-src-5.13.1.tar.xz`
 5. Go there: `cd qt-everywhere-src-5.13.1`
 6. Configure Qt. *Important: replace `username` with your username*. (Choose only one)
        - [Click here to configure a dynamic build, to run Trader on your machine](https://gist.githubusercontent.com/tryphe/acfa2aab0967ee5c99a3d9cee45637cd/raw/28d44f022e2f0236f4776bd40a1acaf0c7500901)
        - [Click here to configure a static build, to copy Trader to another machine with the same OS](https://gist.githubusercontent.com/tryphe/28c3c8407775a5da8936d6afaa97ba7f/raw/f14d7ae9a8abca4c54e689930c4ce2b2fdb43f97)
 7. If there are no errors, run make: `make -j` (if low on RAM or single CPU, you can skip the `-j`)
 8. If there are no errors, run `make install`
 9. Now, run `qmake -v` prefixed with your install path (type the whole thing): `~/localqt/Qt-5.13.1-minimal/bin/qmake -v`
        ~~~
        QMake version 3.1
        Using Qt version 5.13.1 in /home/username/localqt/Qt-5.13.1-minimal/lib
        ~~~
        If you are having problems with step 9 and interfering Qt versions, [invoke qmake using qtchooser instead.](https://gist.github.com/tryphe/5333144b1b9847fe65b8740eceaed14e)

Compiling
---------
 1. Open your source directory, ie. `cd ~/src`
 2. Clone this repo: `git clone https://github.com/tryphe/trader`
 3. Go there: `cd trader`
 4. Hardcode your API keys into `daemon/keydefs.h` (choose only one):
        - Run `python generate_keys.py`
        - or: Copy the example keydefs file: `cp daemon/keydefs.h.example daemon/keydefs.h` then paste your keys in with your favorite editor.
        - or: (*non-hardcoded keys*): Create `keydefs.h` using the example file above, but leave the keys blank or as-is during compile. Then, each time you run the binary, modify the bulk input file `<config-directory>/in.txt`, adding this line: `setkeyandsecret <key> <secret>`.
 5. Run qmake: `~/localqt/Qt-5.13.1-minimal/bin/qmake`
 6. Compile: `make -j`, or `make -jn` where `n` is the number of simultaneous makes.


Updating
--------
Run `git pull` to pull the latest code, then repeat step 6 from the Compiling section. You should do this frequently - minor bugfixes are common.

Running the daemon
------------------
Sessionize traderd so it stays running: `setsid ./traderd`

Config directory
----------------
`~/.config/trader`

Tailing the logs (note: CLI output goes to the logs)
----------------------------------------------------
Running the daemons and relying terminal output is suboptimal if the terminal closes. It's enabled by default, but can be disabled in `daemon/build-config.h`. All output is also routed to the logfiles. There's a color log, and a noncolor log. To tail, run:
`tail -f --lines=200 ~/.config/trader/log.<timestamp>_color.txt`.

Commands
--------
Check out the [list of commands.](https://github.com/tryphe/trader/blob/master/doc/commands.md#formatting) (warning: most of these commands are now deprecated)

FAQS
----
**What the bot is NOT**

The bot doesn't use common chart indicators, like momentum indicators or standard deviation formulas. Instead, it relies on user-generated ping-pong, and automatic ping-pong based on a cross-market tit-for-tat strategy (coming soon).

**What about supporting xyz exchange?**

It's fairly easily to integrate the bot with any API, as long as it can read its current orders and get ticker prices. Currently, it would take roughly 1000 lines to add another exchange. If you know of a good exchange, let me know.
