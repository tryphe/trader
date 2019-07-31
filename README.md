**Prerequisite: Compiling Qt (unix/linux)**
 1. Open your source directory, ie. `cd ~/src`. If it doesn't exist, run `mkdir ~/src` first.
 3. Pull a currently maintained Qt source (Please don't use ancient Qt versions, less than 5.10 is not supported currently): `wget https://download.qt.io/archive/qt/5.13/5.13.0/single/qt-everywhere-src-5.13.0.tar.xz`
 4. Extract: `tar xf qt-everywhere-src-5.13.0.tar.xz`
 5. Go there: `cd qt-everywhere-src-5.13.0.tar.xz`
 6. Install dependencies: `sudo apt-get build-dep qt5-default`
 7. Configure Qt (and skip some things to improve compile time): 
`./configure -prefix /home/username/localqt/Qt-5.13.0-minimal/ -opensource -confirm-license -shared -release -nomake examples -nomake tests -skip qt3d -skip qtactiveqt -skip qtandroidextras -skip qtcanvas3d -skip qtcharts -skip qtdatavis3d -skip qtgamepad -skip qtgraphicaleffects -skip qtlocation -skip qtmultimedia -skip qtpurchasing -skip qtquickcontrols -skip qtquickcontrols2 -skip qtscript -skip qtscxml -skip qtsensors -skip qtserialbus -skip qtserialport -skip qtwebengine -skip qtwebview -skip qttools`
 8. If there are no errors, now you can run make: `make -j`
 9. If there are no errors, run `make install`
 11. Now try and run qmake from your installation path with the `-v` flag: `/home/username/localqt/Qt-5.13.0-minimal/bin/qmake -v` and you should get:
`QMake version 3.1
Using Qt version 5.13.0 in /home/username/localqt/Qt-5.13.0-release/lib`

**Compiling trader(unix/linux)**
 1. Open your source directory, ie. `cd ~/src` 
 2. Clone this repo: `git clone  https://github.com/tryphe/trader`
 3. Put your keys into the bot one of three ways (choose only one):
	- Run `python generate_keys.py` to create `daemon/keydefs.h`
	- or Copy the example keydefs file: `cp daemon/keydefs.h.example daemon/keydefs.h'` then paste your keys in with your favorite editor, or:
	- or Create `keydefs.h` using the example file, but leave the keys blank or as-is during compile, then use the runtime CLI to enter your keys into the bot: `trader-cli Poloniex setkeyandsecret <key> <secret>`
 5. Now that `daemon/keydefs.h` exists, you can build the project from the project's root directory:
 6. Run qmake: `/home/username/localqt/Qt-5.13.0-minimal/bin/qmake`
 7. Build your binaries:
	- Run `./build-all.sh`. This will build all exchange targets and the CLI using.
	- Modify `daemon/build-config.h` with your build preferences and build using `make` or what you like.

**Running the daemon**
1. Daemonize traderd so it stays running: `setsid ./traderd-poloniex`

**Preparing to run trader-cli (unix/linux)**
 1. Edit `~/.bashrc` using your favorite text editor, adding these lines:
`alias trader-cli-poloniex='~/src/trader/trader-cli Poloniex'`
`alias trader-cli-bittrex='~/src/trader/trader-cli Bittrex'`
`alias trader-cli-binance='~/src/trader/trader-cli Binance'`
 19. Then run `source ~/.bashrc`
 20. Now you can access each daemon with a simple command, eg. Poloniex: `trader-cli-poloniex getbalances`
