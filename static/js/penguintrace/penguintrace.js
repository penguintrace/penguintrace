// ----------------------------------------------------------------
// Copyright (C) 2019 Alex Beharrell
//
// This file is part of penguinTrace.
//
// penguinTrace is free software: you can redistribute it and/or
// modify it under the terms of the GNU Affero General Public
// License as published by the Free Software Foundation, either
// version 3 of the License, or any later version.
//
// penguinTrace is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public
// License along with penguinTrace. If not, see
// <https://www.gnu.org/licenses/>.
// ----------------------------------------------------------------
//
// penguinTrace Web UI

function hexHalfword(half)
{
  return ((half & 0xffff) | 0x10000).toString(16).substr(1);
}

function hexString(high, low)
{
  var str = "0x";
  str += hexHalfword(high >> 16) + " ";
  str += hexHalfword(high)       + " ";
  str += hexHalfword(low >> 16)  + " ";
  str += hexHalfword(low);
  return str;
}

function decString(high, low)
{
  if (high == 0)
  {
    return low;
  }
  else if (high == 0xffffffff)
  {
    if (low > 0) return "-"+low;
    else         return low;
  }
  else
  {
    return "-";
  }
}

var ptrace = new Object();

ptrace.languages = {
  c   : {name: "C",        syntax: "text/x-csrc", lang: "c"},
  cxx : {name: "C++",      syntax: "text/x-csrc", lang: "cxx"},
  asm : {name: "Assembly", syntax: "gas",         lang: "asm"}
};

ptrace.architectures = {
  aarch64 : {key: "aarch64", name: "AArch64"},
  x86_64  : {key: "x86_64",  name: "x86_64"}
};

ptrace.supportedArchitectures = [
  ptrace.architectures.aarch64,
  ptrace.architectures.x86_64
];

ptrace.supportedLanguages = [
  ptrace.languages.c,
  ptrace.languages.cxx,
  ptrace.languages.asm
];

ptrace.currentLanguage = null;
ptrace.menuDone = false;

ptrace.autoStep = false;
ptrace.autoStepTimer = null;
ptrace.autoStepDelay = 1000;

ptrace.stopEndpoint = "/stop/";
ptrace.stepEndpoint = "/step/";
ptrace.stepLineEndpoint = "/step-line/";
ptrace.continueEndpoint = "/continue/";
ptrace.compileEndpoint = "/compile/";
ptrace.bkptEndpoint = "/breakpoint/";
ptrace.pollStepEndpoint = "/step-state/";
ptrace.pollSessionEndpoint = "/session-state/";
ptrace.stdinEndpoint = "/stdin/";
ptrace.uploadEndpoint = "/upload/";
ptrace.downloadEndpoint = "/download/";

// Main state of UI
//  INIT  - Before initialisation
//  IDLE  - Editing code
//  DEBUG - Active session
ptrace.state = "INIT";
ptrace.popupWindow = null;
ptrace.sourceCode = null;
ptrace.compiledCode = null;
ptrace.sourceCodeLine = 0;
ptrace.compiledCodeLine = 0;

ptrace.sessionName = "";

// Compiled program information
ptrace.sourceWidgets = new Array();
ptrace.sourceHighlighted = new Array();
ptrace.compiledWidgets = new Array();
ptrace.compiledHighlighted = new Array();
ptrace.disasmMap = new Object();
ptrace.sectionMap = new Object();
ptrace.symbolMap = new Object();
ptrace.lineNumbers = new Array();
ptrace.lineDisasm = new Array();

ptrace.lastRegs = new Object();
ptrace.prevRegs = new Object();
ptrace.lastVars = new Object();
ptrace.lastStack = new Array();
ptrace.pollTries = 0;
// Corresponds to about 15 seconds
ptrace.maxPollTries = 30;

ptrace.viewAmount = 35;

ptrace.clearAllHighlight = function()
{
  while (ptrace.sourceHighlighted.length > 0)
  {
    ptrace.sourceCode.removeLineClass(ptrace.sourceHighlighted.pop(), "wrap");
  }
  while (ptrace.compiledHighlighted.length > 0)
  {
    ptrace.compiledCode.removeLineClass(ptrace.compiledHighlighted.pop(), "wrap");
  }
  while (ptrace.sourceWidgets.length > 0)
  {
    ptrace.sourceWidgets.pop().clear();
  }
  while (ptrace.compiledWidgets.length > 0)
  {
    ptrace.compiledWidgets.pop().clear();
  }
}

ptrace.changeState = function(nextState)
{
  if (nextState == "DEBUG")
  {
    $('#menu-upload-file')[0].disabled = true;
    $('#play-button').prop('title', "Step by Instruction");
    $('#play-button').prop('alt', "Step by Instruction");
    $('#fwd-button').removeClass("disabled");
    $('#fwd-skip-button').removeClass("disabled");
    $('#stop-button').removeClass("disabled");
    $('#loop-button').removeClass("disabled");
    $('#download-button').removeClass("disabled");
  }
  else
  {
    $('#menu-upload-file')[0].disabled = false;
    ptrace.clearAllHighlight();
    ptrace.compiledCode.clearGutter('breakpoints');
    ptrace.sourceCode.clearGutter('breakpoints');

    $('#play-button').prop('title', "Compile");
    $('#play-button').prop('alt', "Compile");
    $('#fwd-button').addClass("disabled");
    $('#fwd-skip-button').addClass("disabled");
    $('#stop-button').addClass("disabled");
    $('#loop-button').addClass("disabled");
    $('#download-button').addClass("disabled");
    $('#loop-button').attr("src", "img/loop.svg");
    ptrace.autoStep = false;
    if (ptrace.sessionName.length > 0)
    {
      // Only clear if there is a previous session
      ptrace.sessionName = "";
      window.location.hash = ptrace.sessionName;
    }
  }

  ptrace.state = nextState;
}

ptrace.requestFailure = function(jqXHR, textStatus, errorThrown)
{
  $('#console-history ul').append("<li class=\"stderr\">Error communicating with server</li>");

  ptrace.changeState("IDLE");
}

ptrace.makeMarker = function () {
  var marker = document.createElement("div");
  marker.style.color = "#882222";
  marker.innerHTML = "●";
  return marker;
}

ptrace.breakpoint = function (set, byLine, at) {
  if (this.state == "DEBUG")
  {
    if (!byLine && isNaN(parseInt(at, 16)))
    {
      console.log("Selected line doesn't have a valid address");
    }
    var data = {"set": set};
    if (byLine)
    {
      data["line"] = at;
    }
    else
    {
      data["addr"] = at.replace(/^0x/, "");
    }
    var endpoint = ptrace.bkptEndpoint + "?sid=" + ptrace.sessionName;
    $.post(endpoint, data, function(resp) {
      if (resp.bkpt)
      {
        ptrace.breakpointUpdate(resp);
      }
      if (resp.error)
      {
        ptrace.requestFailure();
      }
    }, 'json').fail(ptrace.requestFailure);
  }
  else
  {
    console.log("Can't modify breakpoints while not debugging");
  }
}

ptrace.createCodeEditor = function (id, readOnly)
{
  var elem = $('#'+id)[0];
  var code = CodeMirror.fromTextArea(elem, {
    lineNumbers: true,
    readOnly: readOnly,
    gutters: ["CodeMirror-linenumbers", "breakpoints"],
    viewportMargin: Infinity
  });
  return code;
}

ptrace.updateRegsInPopup = function(regs)
{
  if (ptrace.popupWindow !== null && !ptrace.popupWindow.closed)
  {
    ptrace.updateRegisters(ptrace.popupWindow.$, regs);
  }
  ptrace.updateRegisters($, regs);
  regs.forEach(function (elem) {
    ptrace.prevRegs[elem.name] = {high: elem.high, low: elem.low};
  });
}

ptrace.updateVarsInPopup = function(vars)
{
  if (ptrace.popupWindow !== null && !ptrace.popupWindow.closed)
  {
    ptrace.updateVariables(ptrace.popupWindow.$, vars);
  }
  ptrace.updateVariables($, vars);
}

ptrace.updateStackInPopup = function(vars)
{
  if (ptrace.popupWindow !== null && !ptrace.popupWindow.closed)
  {
    ptrace.updateStack(ptrace.popupWindow.$, vars);
  }
  ptrace.updateStack($, vars);
}

ptrace.createPopup = function()
{
  ptrace.popupWindow = window.open("popup.html", "DetailWindow",
    "menubar=no,width=500,height=500");

  ptrace.popupWindow.onload = function() {
    ptrace.popupWindow.doLoad();

    ptrace.updateRegsInPopup(ptrace.lastRegs);
    ptrace.updateVarsInPopup(ptrace.lastVars);
    ptrace.updateStackInPopup(ptrace.lastStack);
  };
}

ptrace.commonStateUpdate = function(data)
{
  if (!data.done && (data.pc in ptrace.disasmMap))
  {
    var line = ptrace.disasmMap[data.pc].line;

    var lhndl = ptrace.compiledCode.addLineClass(line, "wrap", "highlight");
    ptrace.compiledHighlighted.push(lhndl);

    ptrace.compiledCode.scrollIntoView({line: line, ch: 0}, ptrace.viewAmount);
    ptrace.compiledCodeLine = line;
  }

  if ("regs" in data)
  {
    ptrace.lastRegs = data.regs;
    ptrace.updateRegsInPopup(data.regs);
  }
  if ("vars" in data)
  {
    ptrace.lastVars = data.vars;
    ptrace.updateVarsInPopup(data.vars);
  }
  if ("stacktrace" in data)
  {
    ptrace.lastStack = data.stacktrace;
    ptrace.updateStackInPopup(ptrace.lastStack);
  }

  ptrace.breakpointUpdate(data);

  if (!data.done && ("location" in data))
  {
    var line = data['location']['line']-1;
    var column = data['location']['column'];
    var rpt = column > 0 ? column-1 : column;
    var elem = document.createElement("div");
    elem.innerHTML = "&nbsp;".repeat(rpt)+"<b>&#x25b2;</b>";
    elem.className = "highlight-widget";
    var lw = ptrace.sourceCode.addLineWidget(line, elem, {coverGutter: false, noHScroll: true});
    // data.failures[i].category/desc/column
    ptrace.sourceWidgets.push(lw);

    var lhndl = ptrace.sourceCode.addLineClass(line, "wrap", "highlight");
    ptrace.sourceHighlighted.push(lhndl);

    ptrace.sourceCode.scrollIntoView({line: line, ch: column}, ptrace.viewAmount);
    ptrace.sourceCodeLine = line;
  }
  if ("stdout" in data)
  {
    for (var i = 0; i < data.stdout.length; i++)
    {
      $('#console-history ul').append("<li class=\"stdout\">"+data.stdout[i]+"</li>");
    }
  }
  if (data.done)
  {
    $('#console-history ul').append("<li class=\"stderr\">Program Finished</li>");
    ptrace.stopAction();
  }
}

ptrace.breakpointUpdate = function(data)
{
  if ("bkpts" in data)
  {
    ptrace.compiledCode.clearGutter('breakpoints');

    data.bkpts.forEach(function(pc) {
      ptrace.compiledCode.setGutterMarker(ptrace.disasmMap[pc].line, "breakpoints", ptrace.makeMarker());
    });
  }
  if ("bkptLines" in data)
  {
    ptrace.sourceCode.clearGutter('breakpoints');

    data.bkptLines.forEach(function(n){
      ptrace.sourceCode.setGutterMarker(n-1, "breakpoints", ptrace.makeMarker());
    });
  }
}

ptrace.pollSessionState = function(retry)
{
  ptrace.pollTries = 0;
  var endpoint = ptrace.pollSessionEndpoint + "?sid=" + ptrace.sessionName;
  $.get(endpoint, {}, function(data) {
    if (data.arch && !ptrace.menuDone)
    {
      ptrace.setupMenu(data.arch);
      ptrace.menuDone = true;
    }
    if (data.state)
    {
      if (data.compile)
      {
        if ((data.source.length > 0) && (data.lang.length > 0))
        {
          var lang = ptrace.languages[data.lang];
          ptrace.setLanguage(lang);

          ptrace.sourceCode.setValue(data.source);
          ptrace.sourceCode.setOption('mode', lang.syntax);
        }

        console.log("Compile/Parse success");
        ptrace.disasmMap = new Object();
        ptrace.sectionMap = new Object();
        ptrace.symbolMap = new Object();
        ptrace.lineNumbers = new Array();
        ptrace.lineDisasm = new Array();

        data.sections.forEach(function(elem) {
          ptrace.sectionMap[elem.pc] = elem.name;
        });

        data.symbols.forEach(function(elem) {
          ptrace.symbolMap[elem.pc] = elem.name;
        });

        var contents = "";
        data.disassembly.forEach(function(elem) {
          if (elem.pc in ptrace.sectionMap)
          {
            ptrace.lineNumbers.push(-1);
            ptrace.lineDisasm.push(-1);
            contents += "<"+ptrace.sectionMap[elem.pc]+">\n";
          }
          if (elem.pc in ptrace.symbolMap)
          {
            ptrace.lineNumbers.push(-1);
            ptrace.lineDisasm.push(-1);
            contents += ptrace.symbolMap[elem.pc]+":\n";
          }
          ptrace.disasmMap[elem.pc] = {line: ptrace.lineNumbers.length, dis: elem.dis};
          ptrace.lineNumbers.push(elem.pc);
          ptrace.lineDisasm.push(elem.dis);
          contents += "  "+elem.dis+"\n";
        });
        ptrace.compiledCode.setValue(contents);
        var maxLen = ptrace.lineNumbers[ptrace.lineNumbers.length-1].toString(16).length+2;
        ptrace.compiledCode.setOption('lineNumberFormatter', function(lineNo) {
          if (lineNo > ptrace.lineNumbers.length || ptrace.lineNumbers[lineNo-1] < 0)
          {
            return "-".repeat(maxLen);
          }
          return "0x"+(ptrace.lineNumbers[lineNo-1].toString(16));
        });
        ptrace.compiledCode.refresh();

        ptrace.commonStateUpdate(data);

        ptrace.changeState("DEBUG");
      }
      else
      {
        console.error("Compile failed");
        for (var i = 0; i < data.failures.length; i++)
        {
          if (data.failures[i].hasOwnProperty('line'))
          {
            // Failure from compiler is 1-indexed, editor 0-indexed
            var line = data.failures[i].line-1;
            var col = data.failures[i].column;
            var cat = data.failures[i].category;
            var desc = data.failures[i].desc;
            var elem = document.createElement("div");
            elem.innerHTML = "<b>"+cat+":</b> "+desc;
            elem.className = "compile-error";
            var lhndl = ptrace.sourceCode.addLineClass(line, "wrap", "compile-error");
            var lw = ptrace.sourceCode.addLineWidget(line, elem, {coverGutter: false, noHScroll: true});
            // data.failures[i].category/desc/column
            ptrace.sourceHighlighted.push(lhndl);
            ptrace.sourceWidgets.push(lw);

            ptrace.sourceCode.scrollIntoView({line: line, ch: col}, ptrace.viewAmount);
            ptrace.sourceCodeLine = line;
          }
          else
          {
            $('#console-history ul').append("<li class=\"stderr\">"+data.failures[i].category+"</li>");
            $('#console-history ul').append("<li class=\"stderr\">"+data.failures[i].desc+"</li>");
          }
        }
      } /* data.compile */
    } /* data.state */
    else
    {
      if (retry || data.retry)
      {
        if (ptrace.pollTries > ptrace.maxPollTries)
        {
          ptrace.requestFailure();
        }
        else
        {
          ptrace.pollTries++;
          setTimeout(ptrace.pollSessionStateRetry, 500);
        }
      }
    }
  }, 'json').fail(ptrace.requestFailure);
}

ptrace.pollSessionStateRetry = function ()
{
  ptrace.pollSessionState(true);
}

ptrace.pollSessionStateNoRetry = function ()
{
  // Will still retry if there are pending commands
  ptrace.pollSessionState(false);
}

ptrace.compileAction = function()
{
  var endpoint = ptrace.compileEndpoint+"?lang="+ptrace.currentLanguage.lang;
  endpoint += "&args="
  endpoint +=  encodeURIComponent($('#menu-input-text')[0].value);
  $.post(endpoint, ptrace.sourceCode.getValue(), function(data) {
    ptrace.clearAllHighlight();
    if (data.compile)
    {
      ptrace.sessionName = data.session;
      window.location.hash = ptrace.sessionName;
      console.log("Compile request request success");
      setTimeout(ptrace.pollSessionStateRetry, 500);
    }
    else
    {
      ptrace.requestFailure();
    }
  }, 'json').fail(ptrace.requestFailure);
}

ptrace.pollStepState = function()
{
  ptrace.pollTries = 0;
  var endpoint = ptrace.pollStepEndpoint + "?sid=" + ptrace.sessionName;
  $.get(endpoint, {}, function(data) {
    if (data.state)
    {
      if (data.step)
      {
        ptrace.clearAllHighlight();

        var stepInfo = "0x"+data.pc.toString(16);
        stepInfo += ": "+data.disasm;
        console.log(stepInfo);

        ptrace.commonStateUpdate(data);

        if (ptrace.autoStep && !data.done)
        {
          ptrace.autoStepTimer = setTimeout(function() {
            ptrace.stepAction("instruction");
          }, ptrace.autoStepDelay);
        }
      }
      else
      {
        ptrace.requestFailure();
      }
    }
    else
    {
      if (ptrace.pollTries > ptrace.maxPollTries)
      {
        ptrace.requestFailure();
      }
      else
      {
        ptrace.pollTries++;
        setTimeout(ptrace.pollStepState, 500);
      }
    }
  }, 'json').fail(ptrace.requestFailure);
}

ptrace.stepAction = function(step)
{
  // Only clear highlighting when response received
  // Request to step endpoint
  var stepReq = {};
  var endpoint;
  if (step == "instruction")
  {
    endpoint = ptrace.stepEndpoint + "?sid=" + ptrace.sessionName;
  }
  else if (step == "line")
  {
    endpoint = ptrace.stepLineEndpoint + "?sid=" + ptrace.sessionName;
  }
  else if (step == "continue")
  {
    endpoint = ptrace.continueEndpoint + "?sid=" + ptrace.sessionName;
  }
  else
  {
    console.error("Unknown step type");
  }

  $.post(endpoint, stepReq, function(data) {
    if (data.step)
    {
      console.log("Step request ok");
      setTimeout(ptrace.pollStepState, 100);
    }
    else
    {
      ptrace.requestFailure();
    }
  }).fail(ptrace.requestFailure);
}

ptrace.stopAction = function()
{
  var endpoint = ptrace.stopEndpoint + "?sid=" + ptrace.sessionName;
  clearTimeout(ptrace.autoStepTimer);
  ptrace.autoStep = false;
  var stopReq = {};
  $.post(endpoint, stopReq, function() {
    ptrace.changeState("IDLE");
  }).fail(ptrace.requestFailure);

}

ptrace.moreClick = function()
{
  console.log(ptrace.popupWindow);
  if (ptrace.popupWindow === null || ptrace.popupWindow.closed)
  {
    ptrace.createPopup();
  }
}

ptrace.playClick = function()
{
  if (ptrace.state == "INIT")
  {
    console.error("Not yet initialised");
  }
  else if (ptrace.state == "IDLE")
  {
    ptrace.compileAction();
  }
  else if (ptrace.state == "DEBUG")
  {
    ptrace.stepAction("instruction");
  }
  else
  {
    console.error("Unknown state");
  }
}

ptrace.fwdClick = function()
{
  if (ptrace.state == "DEBUG")
  {
    ptrace.stepAction("continue");
  }
}

ptrace.fwdSkipClick = function()
{
  if (ptrace.state == "DEBUG")
  {
    ptrace.stepAction("line");
  }
}

ptrace.stopClick = function()
{
  if (ptrace.state == "DEBUG")
  {
    ptrace.stopAction();
  }
}

ptrace.downloadClick = function()
{
  if (ptrace.state == "DEBUG")
  {
    window.location.href = ptrace.downloadEndpoint + "?sid=" + ptrace.sessionName;
  }
}

ptrace.setupButtonActions = function()
{
  $('#more-button').click(ptrace.moreClick);
  $('#play-button').click(ptrace.playClick);
  $('#fwd-button').click(ptrace.fwdClick);
  $('#fwd-skip-button').click(ptrace.fwdSkipClick);
  $('#stop-button').click(ptrace.stopClick);
  $('#download-button').click(ptrace.downloadClick);
  $('#menu-close').click(function(){
    $('#menu-background').css('display', 'none');
  });
  $('#menu-button').click(function(){
    $('#menu-background').css('display', 'block');
  });
  $('#loop-button').click(function() {
    if (ptrace.autoStep)
    {
      $('#loop-button').attr("src", "img/loop.svg");
      ptrace.autoStep = false;
    }
    else
    {
      ptrace.stepAction("instruction");
      $('#loop-button').attr("src", "img/pause.svg");
      ptrace.autoStep = true;
    }
  });
}

ptrace.setupBreakpointActions = function()
{
  this.sourceCode.on("gutterClick", function(cm, n) {
    var info = cm.lineInfo(n);
    var setBkpt = !info.gutterMarkers || !('breakpoints' in info.gutterMarkers);
    ptrace.breakpoint(setBkpt, true, n+1);
  });
  this.compiledCode.on("gutterClick", function(cm, n) {
    var info = cm.lineInfo(n);
    var setBkpt = !info.gutterMarkers || !('breakpoints' in info.gutterMarkers);
    var pc = ptrace.compiledCode.getOption('lineNumberFormatter')(n+1);
    ptrace.breakpoint(setBkpt, false, pc);
  });
}

ptrace.setupStdinActions = function()
{
  $('#console-input-text').keypress(function (e) {
    if (e.key == "Enter")
    {
      var text = $(e.currentTarget)[0].value;

      if (ptrace.state == "DEBUG")
      {
        $.post(ptrace.stdinEndpoint, text, function(data) {
          if (data.stdin)
          {
            $('#console-history ul').append("<li class=\"stdin\">"+text+"</li>");
            $('#console-history').scrollTop($('#console-history ul').height());
          }
          else
          {
            ptrace.requestFailure();
          }
          $(e.currentTarget)[0].value = "";
        }, 'json').fail(ptrace.requestFailure);
      }
      else
      {
        $(e.currentTarget)[0].value = "";
      }
    }
  });

  $('#console-history-clear').click(function () {
    $('#console-history ul li').remove();
  });
}

ptrace.uploadFileHandler = function(event)
{
  var fl = event.target.files;
  if (fl.length > 0)
  {
    var reader = new FileReader();

    reader.addEventListener("load", function () {
      $.post(ptrace.uploadEndpoint, reader.result.split(',')[1], function(data) {
        ptrace.clearAllHighlight();
        if (data.compile)
        {
          ptrace.sessionName = data.session;
          window.location.hash = ptrace.sessionName;
          console.log("Upload request request success");
          $('#menu-background').css('display', 'none');
          // Clear uploaded file
          $('#menu-upload-file')[0].value = "";
          setTimeout(ptrace.pollSessionStateRetry, 500);
        }
        else
        {
          ptrace.requestFailure();
        }
      }, 'json').fail(ptrace.requestFailure);
    }, false);

    var f = fl[0];
    reader.readAsDataURL(f);
  }
}

ptrace.setupMenu = function(arch)
{
  $('#menu-lang').empty();
  $('#menu-code').empty();
  ptrace.supportedLanguages.forEach(function(lang)
  {
    var elem = $("<li id='lang-"+lang.lang+"'>"+lang.name+"</li>");
    elem.click(function() {
      ptrace.setLanguage(lang);
    });
    $('#menu-lang').append(elem);
  });
  for (var key in codeExamples)
  {
    (function() {
      var code = codeExamples[key];
      if ('arch' in code && arch.length > 0 && code.arch.key != arch) return;
      var name = code.name+"<span class='code-lang'> ("+code.lang.name;
      if ('arch' in code)
      {
        name += " - "+code.arch.name;
      }
      name += ")</span><br /><div class='code-desc'>";
      name += code.description;
      name += "</div>"
      var elem = $("<li>"+name+"</li>");
      elem.click(function() {
        var c = code;
        ptrace.loadCode(c);
        $('#menu-background').css('display', 'none');
      });
      $('#menu-code').append(elem);
    })();
  }
}

ptrace.onDropdown = function(e)
{
  var selected = e.target.value;
  ptrace.onTabClick(selected);
}

ptrace.onTabClick = function(selected)
{
  $('#code-wrapper div.code').removeClass('inactive');
  $('#code-wrapper div').removeClass('wide-inactive');
  $('#code-wrapper div.code').addClass('inactive');
  $('#code-wrapper div#'+selected).removeClass('inactive');
  $('#controls-tabs-wide li').removeClass('active');
  if (selected == 'main-code-wrapper')
  {
    ptrace.sourceCode.refresh();
    ptrace.compiledCode.refresh();
    ptrace.sourceCode.scrollIntoView({line: ptrace.sourceCodeLine, ch: 0}, ptrace.viewAmount);
    ptrace.compiledCode.scrollIntoView({line: ptrace.compiledCodeLine, ch: 0}, ptrace.viewAmount);
    $('#controls-tabs-wide li#main-code-wrapper-button').addClass('active');
  }
  else if (selected == 'asm-code-wrapper')
  {
    ptrace.compiledCode.refresh();
    ptrace.compiledCode.scrollIntoView({line: ptrace.compiledCodeLine, ch: 0}, ptrace.viewAmount);
    $('#controls-tabs-wide li#main-code-wrapper-button').addClass('active');
  }
  else
  {
    $('#code-wrapper div#main-code-wrapper').addClass('wide-inactive');
    $('#code-wrapper div#asm-code-wrapper').addClass('wide-inactive');
    $('#controls-tabs-wide li#'+selected+'-button').addClass('active');
  }
  $('#controls-tabs-select')[0].value = selected;
}

ptrace.setupDropdown = function()
{
  $('#controls-tabs-select')[0].value = "main-code-wrapper";
  $('#controls-tabs-select').change(ptrace.onDropdown);
  $('#main-code-wrapper-button').click(function() {
    ptrace.onTabClick('main-code-wrapper');
  });
  $('#code-regs-button').click(function() {
    ptrace.onTabClick('code-regs');
  });
  $('#code-vars-button').click(function() {
    ptrace.onTabClick('code-vars');
  });
  $('#code-mem-button').click(function() {
    ptrace.onTabClick('code-mem');
  });
}

ptrace.prevWidth = 0;

ptrace.resizeHandle = function(e)
{
  var newWidth = e.target.innerWidth;

  if (ptrace.prevWidth != 0)
  {
    if (((ptrace.prevWidth <= 750) && (newWidth  > 750)) ||
        ((ptrace.prevWidth  > 750) && (newWidth <= 750)))
    {
      ptrace.refreshHandle();
    }
  }

  ptrace.prevWidth = newWidth;
}

ptrace.refreshHandle = function()
{
  ptrace.sourceCode.refresh();
  ptrace.compiledCode.refresh();
}


ptrace.startApp = function()
{
  ptrace.sourceCode = this.createCodeEditor('source-code', false);
  ptrace.compiledCode = this.createCodeEditor('compiled-code', true);
  ptrace.compiledCode.setOption('mode', 'gas');

  ptrace.setupButtonActions();
  ptrace.setupBreakpointActions();
  ptrace.setupStdinActions();
  ptrace.setupMenu("");
  ptrace.setupDropdown();

  $('#menu-upload-file').change(ptrace.uploadFileHandler);

  $(window).resize(ptrace.resizeHandle);
  window.addEventListener("orientationchange", ptrace.refreshHandle, false);

  ptrace.changeState("IDLE");

  ptrace.sessionName = window.location.hash.slice(1);

  ptrace.pollSessionStateNoRetry();
}

ptrace.setLanguage = function(lang)
{
  ptrace.currentLanguage = lang;
  $('#menu-lang li').removeClass('selected');
  $('#lang-'+lang.lang).addClass('selected');
}

ptrace.loadCode = function(code)
{
  ptrace.setLanguage(code.lang);

  ptrace.sourceCode.setValue(code.code);
  ptrace.sourceCode.setOption('mode', code.lang.syntax);
}

ptrace.updateRegisters = function(jq, regs)
{
  jq('.reg-changed').removeClass('reg-changed');
  var tbody = jq('#registers-tbody');
  regs.forEach(function (elem) {
    var row = "<tr id=\""+elem.name+"\">";
    row += "<td class=\"first-row\">";
    row += elem.name;
    row += "</td><td>";
    row += hexString(elem.high, elem.low);
    row += "</td><td>";
    row += decString(elem.high, elem.low);
    row += "</td></tr>";
    var trow = jq('#'+elem.name);
    if (trow.length > 0)
    {
      trow.replaceWith(row);
      if (elem.name in ptrace.prevRegs)
      {
        var pVal = ptrace.prevRegs[elem.name];
        if ((elem.high != pVal.high) || (elem.low != pVal.low))
        {
          jq('#'+elem.name).addClass('reg-changed');
        }
      }
    }
    else
    {
      tbody.append(row);
    }
  });
}

ptrace.updateVariables = function(jq, vars)
{
  var tbody = jq('#variables-tbody');
  tbody.html("");
  vars.forEach(function (elem) {
    var row = "<tr id=\""+elem.name+"\">";
    row += "<td class=\"first-row\">";
    row += elem.name;
    row += "</td><td>";
    row += elem.value;
    row += "</td></tr>";
    tbody.append(row);
  });
}

ptrace.updateStack = function(jq, vars)
{
  var tbody = jq('#memory-tbody');
  tbody.html("");
  for (var i = 0; i < vars.length; i++)
  {
    var row = "<tr><td>";
    row += vars[i];
    row += "</td></tr>";
    tbody.append(row);
  }
}