var mouseHeld = false;
var enabling = false;
var segStates = {};

function init() {
    $("path").on("mousedown", onPathMouseDown);
    $("path").on("mouseenter", onPathMouseEnter);
    $(document).on("mousedown", onMouseDown);
    $(document).on("mouseup", onMouseUp);
    //$("#pattern-bytes").click(function(e) {$(e.target).select();})
    $("#btn-clear").on("click", clearSegments);
    $("#btn-load").on("click", updateSVGFromBitPattern);
    $("#btn-shift-left").on("click", shiftLeft);
    $("#btn-shift-right").on("click", shiftRight);
    clearSegments()
}

function setSegmentState(id, state) {
    let svgSeg = $("#seg" + id);
    if (svgSeg.length == 0) return;
    svgSeg.attr("style", state ? "fill: #ff0" : "fill: #555");
    svgSeg.attr("data-active", state ? "1" : "0");
}

function getSegmentState(id) {
    let svgSeg = $("#seg" + id);
    if (svgSeg.length == 0) return false;
    return (svgSeg.attr("data-active") != "0");
}

function getSegmentId(element) {
    let segId = parseInt(element.attr("id").replace("seg", ""));
    if (isNaN(segId)) return null;
    return segId;
}

function segmentExists(id) {
    return ($("#seg" + id).length > 0);
}

function getPatternBytes() {
    let patternBytes = [0, 0, 0, 0, 0, 0, 0, 0];
    for (var seg = 0; seg <= 58; seg++) {
        let byte = Math.floor(seg / 8);
        let bit = 7 - (seg % 8);
        if (segStates[seg]) patternBytes[byte] |= (1 << bit);
    }
    return patternBytes;
}

function patternBytesToPatternText(patternBytes) {
    let patternText = "";
    for (var i = 0; i < 8; i++) {
        patternText += "0x" + ("00" + patternBytes[i].toString(16).toUpperCase()).slice(-2) + ", ";
    }
    return patternText;
}

function patternTextToPatternBytes(patternText) {
    let bytesStr = patternText.split(", ");
    let patternBytes = [];
    for (var byteStr of bytesStr) {
        let byte = parseInt(byteStr);
        if (isNaN(byte)) continue;
        patternBytes.push(byte);
    }
    return patternBytes;
}

function onPathMouseDown(e) {
    let path = $(e.target);
    let segId = getSegmentId(path);
    if (!segId) return;
    let active = getSegmentState(segId);
    enabling = !active;
    active = !active;
    setSegmentState(segId, active);
    updateBitPatternFromSVG();
}

function onPathMouseEnter(e) {
    if (!mouseHeld) return;
    let path = $(e.target);
    let segId = getSegmentId(path);
    if (!segId) return;
    setSegmentState(segId, enabling);
    updateBitPatternFromSVG();
}

function onMouseDown() {
    mouseHeld = true;
}

function onMouseUp() {
    mouseHeld = false;
}

function updateSegStatesFromSVG() {
    for (var _path of $("#svg").find("path")) {
        let path = $(_path);
        let segId = getSegmentId(path);
        if (!segId) continue;
        let active = getSegmentState(segId);
        segStates[segId] = active;
    }
}

function updateSVGFromSegStates() {
    for (var seg = 0; seg <= 58; seg++) {
        setSegmentState(seg, segStates[seg]);
    }
}

function updateBitPatternFromSVG() {
    updateSegStatesFromSVG();
    let patternBytes = getPatternBytes();
    let patternText = patternBytesToPatternText(patternBytes);
    $("#pattern-bytes").val(patternText);
}

function updateSVGFromBitPattern() {
    let text = $("#pattern-bytes").val();
    let patternBytes = patternTextToPatternBytes(text);
    if (patternBytes.length < 8) return;
    
    for (var seg = 0; seg <= 58; seg++) {
        if (!segmentExists(seg)) continue;
        let byte = Math.floor(seg / 8);
        let bit = 7 - (seg % 8);
        let active = !!(patternBytes[byte] & (1 << bit));
        segStates[seg] = active;
    }
    updateSVGFromSegStates();
}

function clearSegments() {
    for (var _path of $("#svg").find("path")) {
        let path = $(_path);
        if (!path.attr("id").startsWith("seg")) continue;
        path.attr("style", "fill: #555");
        path.attr("data-active", "0");
        updateBitPatternFromSVG();
    }
}

function shiftLeft() {
    let patternBytes = patternTextToPatternBytes($("#pattern-bytes").val());
    let firstMSB = !!(patternBytes[0] & 0x80);
    for (var i = 0; i < patternBytes.length; i++) {
        patternBytes[i] <<= 1;
        let msb = ((i == patternBytes.length - 1) ? firstMSB : !!(patternBytes[i+1] & 0x80));
        patternBytes[i] |= (msb ? 1 : 0);
    }
    $("#pattern-bytes").val(patternBytesToPatternText(patternBytes));
    updateSVGFromBitPattern();
}

function shiftRight() {
    let patternBytes = patternTextToPatternBytes($("#pattern-bytes").val());
    let lastLSB = !!(patternBytes[patternBytes.length - 1] & 0x01);
    for (var i = patternBytes.length; i >= 0; i--) {
        patternBytes[i] >>= 1;
        let lsb = ((i == 0) ? lastLSB : !!(patternBytes[i-1] & 0x01));
        patternBytes[i] |= (lsb ? 0x80 : 0);
    }
    $("#pattern-bytes").val(patternBytesToPatternText(patternBytes));
    updateSVGFromBitPattern();
}

$(document).ready(init);