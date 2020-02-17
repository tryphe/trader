#include "spruceoverseer_test.h"
#include "engine.h"
#include "spruce.h"
#include "spruceoverseer.h"

#include <QDebug>

void SpruceOverseerTest::test( SpruceOverseer *o, Engine *engine )
{
    const QString TEST_MARKET = "TEST_1";

    /// test spruce ticker
    // insert ticker price to pass sanity check
    engine->market_info[ TEST_MARKET ].highest_buy = "0.00010000";
    engine->market_info[ TEST_MARKET ].lowest_sell = "0.00010100";

}
