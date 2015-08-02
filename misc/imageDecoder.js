var getPixels = require('get-pixels');

getPixels("mattegan.png", function(err, pixels) {
    if(err) {
        console.log(err);
    } else {
        var w = 59;
        var h = 10;
        var lineStr;
        var pixels = pixels.data;
        //console.log(pixels);
        for(var col = 0; col < w; col++) {
            lineStr = '\t0b';
            for(var row = 9; row >= 0; row--) {
                var pixelStartIndex = (col + row * w) * 4;
                var color;
                for(var c = 0; c < 3; c++) {
                    color = pixels[pixelStartIndex + c] << ((2 - c) * 8);
                }
                lineStr += color == 0 ? '1' : '0';
                //console.log(value);
            }
            console.log(lineStr + ',');
        }
    }
})
