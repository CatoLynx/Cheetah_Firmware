var mouseHeld = false;
var enabling = false;
var segStates = {};

function init() {
    $("path").on("mousedown", onPathMouseDown);
    $("path").on("mouseenter", onPathMouseEnter);
    $(document).on("mousedown", onMouseDown);
    $(document).on("mouseup", onMouseUp);
    $("#pattern-bytes").click(function(e) {$(e.target).select();})
    $("#btn-clear").on("click", clearSegments);
    clearSegments()
}

function onPathMouseDown(e) {
    let path = $(e.target);
    if (!path.attr("id").startsWith("seg")) return;
    let active = path.attr("data-active") != "0";
    enabling = !active;
    active = !active;
    $(this).attr("style", active ? "fill: #ff0" : "fill: #555");
    $(this).attr("data-active", active ? "1" : "0");
    updateBitPattern();
}

function onPathMouseEnter(e) {
    if (!mouseHeld) return;
    let path = $(e.target);
    if (!path.attr("id").startsWith("seg")) return;
    $(this).attr("style", enabling ? "fill: #ff0" : "fill: #555");
    $(this).attr("data-active", enabling ? "1" : "0");
    updateBitPattern();
}

function onMouseDown() {
    mouseHeld = true;
}

function onMouseUp() {
    mouseHeld = false;
}

function updateBitPattern() {
    for (var _path of $("#svg").find("path")) {
        let path = $(_path);
        if (!path.attr("id").startsWith("seg")) continue;
        let active = path.attr("data-active") != "0";
        let segId = parseInt(path.attr("id").replace("seg", ""));
        segStates[segId] = active;
    }

    let patternBytes = [0, 0, 0, 0, 0, 0, 0, 0];
    for (var seg = 0; seg <= 58; seg++) {
        let byte = Math.floor(seg / 8);
        let bit = 7 - (seg % 8);
        if (segStates[seg]) patternBytes[byte] |= (1 << bit);
    }
    
    let patternText = "";
    for (var i = 0; i < 8; i++) {
        patternText += "0x" + ("00" + patternBytes[i].toString(16).toUpperCase()).slice(-2) + ", ";
    }

    $("#pattern-bytes").val(patternText);
}

function clearSegments() {
    for (var _path of $("#svg").find("path")) {
        let path = $(_path);
        if (!path.attr("id").startsWith("seg")) continue;
        path.attr("style", "fill: #555");
        path.attr("data-active", "0");
        updateBitPattern();
    }
}

$(document).ready(init);