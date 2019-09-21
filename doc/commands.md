Formatting
----------
Command format: `<required> [optional=default_value]`\
Commands are text arguments with spaces in between. If you setup the bash aliases in README.md, you can call them with `<exchange> <command>`. If not, you can use `trader-cli <exchange> <command>`. If you want to give the bot bulk commands, put them in `<config_dir>/in.txt` and save the file, one command per line.

A note about markets
--------------------
There is two accepted market formats for any exchange. They are `BASE-QUOTE` and `BASE_QUOTE` where BASE is the base currency, and QUOTE is the quote currency. For example, this means that if you are buying and selling LTC and BTC, and the market is priced in BTC, you are trading in the `BTCLTC` market. This means `1 LTC = x BTC`, where `x` is the market price of `BTC-LTC`. You can also enter `BTC_LTC` and get the same result. Sidenote: if you want to modify the formatting, change the value of `DEFAULT_MARKET_STRING_TEMPLATE` in `daemon/global.h`

Main bot commands
-----------------
```
setorder <market> <buy|sell> <lo> <hi> <amount> <ghost|active>  - add a new ping-pong index
cancelall [market=all]                          - cancels orders, clears position index, for one or all markets
cancellocal [market=all]                        - cancels orders, clears position index, deletes positions, for one or all markets
savemarket [market=all] [orders_per_side=1]     - save dat market yo
savesettings					- save config to ~/tt/settings.txt
exit/quit/stop
getbalances                                     - (runs an api) get exchange balances
getorders <market>                              - show active positions by price
getordersbyindex <market>                       - show active positions by index
getshortlong <tag>				- print short/long total for tag
getdailyvolume                                  - print total volume per day
getvolume                                       - print each market volume and total volume
getfills                                        - print # of fills for each market
getdailymarketvolume                            - print market volume for each [day, market]
getbuyselltotal                                 - print local order count
gethibuylosell                                  - print market spreads
```

Ping-pong options
-----------------
```
setordermax <market> <max count>                - set automated max order count for market. 0 will disable automation of market
setordermin <market> <min count>                - set automated min order count for market. 0 will disable automation of market
setorderdc <market> <count>                     - set diverge/converge count for each landmark order
setorderdcnice <market> <nice>
setorderlandmarkstart <market> <n>              - offset from hi_buy to start diverge/converge
setorderlandmarkthresh <market> <n>             - offset from order_max to start landmark sets
setmarketsentiment <market> <bool>              - true = bullish, false = bearish
setmarketoffset <market> <offset>               - set offset for market. 0.003 = 0.3% = 0.15% compensation per fill
```

Local options
-------------
```
setkeyandsecret <key_plain> <secret_plain>      - manually set key
setkeyandsecrethex <hex blob> <hex blob>        - manually set key by hex value
setfee <double>                                 - manually set fee (changes automatically on polo)
setslippagemultiplier <real>                    - price guessing after post-only error. increment = (mul*p)+0.00000001
setnaminterval <ms>                             - set timer interval for packets
setbookinterval <ms>                            - set timer interval for orderbook updates
setpublicbookinterval <ms>                      - slippage calc price update interval
setcheckinterval <ms>                           - set timer interval for timeout/buysellcount
setdcinterval <ms>
setsentcommandsmax <n>                          - limit the number of in-flight commands
setcancelthresh <n>                             - upper limit for cancel loop
```

Misc and testing - be careful!
------------------------------
```
getbuildversion                                 - print build version
setclearstrayorders <bool>                      - detect stray orders and cancel invalid duplicates(also required for scan-set)
setgracetimelimit <ms>                          - threshhold to clear stray orders after detection, in seconds
setslippagecalculated <bool>                    - try calculated slippage before additive. if disabled, additive+additive2 only
setadjustbuysell <bool>                         - allow post-only error price to correct hi_buy/lo_sell price for slippage calculations
setpostonly <bool>                              - use maker orders only for automatic orders
setdcslippage <bool>                            - include slippage orders in diverge/converge function
setsafetydelaytime <ms>                         - tolerance for order_set_time and orderbook_receive_time
setrequesttimeout <ms>
setcanceltimeout <ms>
setslippagetimeout <ms>
setslippagestaletime <ms>                       - delay inclusion of slippage prices into slippage calculations for n seconds after set
clearstratstats                                 - clear stats for market
clearallstats                                   - clear all stats
getconfig                                       - show the current internal settings and trading preferences
sendcommand <command> <url-args>                - send manual api command
cancellowest <market>                           - cancel lowest local order by price
cancelhighest <market>                          - cancel highest local order by price
setnextlowest <market> [buy|sell]               - set new lowest order from stored indexes. optional override for buy/sell
setnexthighest <market> [buy|sell]              - set new highest order from stored indexes. optional override for buy/sell
short <market>                                  - short by price
shortindex <market>                             - short by index
long <market>                                   - long by price
longindex <market>                              - long by index
```
