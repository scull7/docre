
[@bs.val] [@bs.scope "ocaml"] external reason_compile : string => 'result = "reason_compile_super_errors";
[@bs.val] [@bs.scope "ocaml"] external ocaml_compile : string => 'result = "compile_super_errors";
[@bs.val] [@bs.scope "Colors"] external parseAnsi : string => {. "spans": array({. "text": string, "css": string})} = "parse";
[@bs.val] external eval: string => 'a = "";

[@bs.val] external bsRequirePaths: Js.Dict.t(string) = "";
[@bs.val] external packRequire: string => 'package = "";

[@bs.set "value"] external setInput: Dom.element => string => unit = "";
[@bs.send] external select: Dom.element => unit = "";
[@bs.scope "document"] [@bs.val] external execCommand: string => unit = "";

[@bs.val] [@bs.scope "location"] external origin: string = "";
[@bs.val] [@bs.scope "location"] external pathname: string = "";
[@bs.val] [@bs.scope "location"] external href: string = "";

[@bs.val] [@bs.module "lz-string"] external compress: string => string = "compressToEncodedURIComponent";
[@bs.val] [@bs.module "lz-string"] external decompress: string => string = "decompressFromEncodedURIComponent";
[@bs.val] [@bs.scope "history"] external replaceState: Js.Dict.t('a) => string => string => unit = "";

let replaceState = url => replaceState(Js.Dict.empty(), "", url);

let getInputValue = event => ReactDOMRe.domElementToObj(ReactEventRe.Form.target(event))##value;

type ast;
[@bs.val] external parseML: string => ast = "";
[@bs.val] external printML: ast => string = "";
[@bs.val] external parseRE: string => ast = "";
[@bs.val] external printRE: ast => string = "";

type codemirror;
[@bs.send] external setValue: (codemirror, string) => unit = "";
[@bs.send] external getValue: (codemirror) => string = "";
type textarea = Dom.element;

type window;
[@bs.val] external window: window = "";
[@bs.set] external setCm: (window, codemirror) => unit = "cm";

type jspos = {. "line": int, "ch": int};
[@bs.send] external markText: (codemirror, jspos, jspos, {. "className": string}) => unit = "";

type indexData;
type index;
[@bs.val] external indexData: indexData = "searchindex";
[@bs.val] [@bs.scope "elasticlunr.Index"] external load: indexData => index = "";
let index = load(indexData);
let config = {"bool": "AND", "fields": {"title": {"boost": 2}, "contents": {"boost": 1}}, "expand": true};
type searchResult = {.
  "score": float,
  "doc": {.
    "href": string,
    "title": string,
    "contents": string,
    "rendered": string,
    "breadcrumb": string
  }
};
[@bs.send] external searchIndex: (index, string, 'config) => array(searchResult) = "search";
let searchIndex = text => searchIndex(index, text, config);
/* var index = elasticlunr.Index.load(window.searchindex); */
/* var config = {bool: 'AND', fields: {title: {boost: 2}, contents: {boost: 1}}, expand: true}; */

let clearMarks: codemirror => unit = [%bs.raw {|
  (function(cm) {
    cm.getAllMarks().forEach(mark => {
      cm.removeLineWidget(mark)
    })
  })
|}];

type completionItem = {.
  "docs": Js.nullable(string),
  "kind": string,
  "name": string,
  "path": string,
  "_type": string,
};

let findOpens = text => {
  let opens = [||];

  let rec findBack = (char, i) => {
    if (i < 0) { 0 } else if (text.[i] == char) {
      i - 1
    } else {
      findBack(char, i - 1)
    }
  };

  let rec findOpenComment = (i) => {
    if (i < 1) { 0 } else if (text.[i] == '*' && text.[i - 1] == '/') {
      i - 2
    } else {
      findOpenComment(i - 1)
    }
  };

  let rec skipWhite = i => if (i < 0) { 0 } else {
    switch (text.[i]) {
    | ' ' | '\n' | '\t' => skipWhite(i - 1)
    | _ => i
    }
  };

  let maybeOpen = i0 => {
    let rec loop = i => {
      if (i < 5) {
        0
      } else {
        switch (text.[i]) {
        | 'a'..'z' | 'A'..'Z' | '.' | '_' | '0'..'9' => loop(i - 1)
        | ' ' => {
          let at = skipWhite(i - 1);
          if (at >= 3 &&
            text.[at - 3] == 'o' &&
            text.[at - 2] == 'p' &&
            text.[at - 1] == 'e' &&
            text.[at] == 'n'
            ) {
            Js.Array.push(Js.String.slice(~from=i + 1, ~to_=i0 + 1, text), opens) |> ignore;
            at - 4
          } else {
            at
          }
        }
        | _ => i
        }
      }
    };
    loop(i0 - 1)
  };

  let rec loop = i => {
    if (i > 0) {
      switch (text.[i]) {
      | '}' => loop(findBack('{', i - 1))
      | '"' => loop(findBack('"', i - 1))
      | 'a'..'z' | 'A'..'Z' | '_' | '0'..'9' => loop(maybeOpen(i))
      | _ => if (i > 1 && text.[i] == '/' && text.[i - 1] == '*') {
          loop(findOpenComment(i - 2))
        } else {
          loop(i - 1)
        }
      }
    }
  };
  loop(String.length(text) - 1) |> ignore;
  opens;
};

let autoComplete: (codemirror, completionItem => unit, unit => unit) => bool = [%bs.raw {|
  (function(cm, onSelect, onClose) {
    var cur = cm.getCursor();
    var t = cm.getTokenTypeAt(cur);
    if (t == 'string' || t == 'number' || t == 'comment') {
      return
    }
    var prev = cm.getRange({line:0,ch:0}, cur)

    // TODO TODO if this is a label, then stop
    // ~pos=px(10)

    var recursiveRemove = (text, re) => {
      var res = text.replace(re, '');
      if (res == text) return res
      return recursiveRemove(res, re)
    }

      /* // multi-line comments
    let oprev = recursiveRemove(prev, /\/*(\*[^\/]|\/[^*]|[^/*])*\*\//g, '')
      // strings
      .replace(/"[^"]*"/g, '')
      // curlys
    oprev = recursiveRemove(oprev, /{[^}]*}/g)
      // brackets
    oprev = recursiveRemove(oprev, /[[^\]]*]/g)
      // parens
    oprev = recursiveRemove(oprev, /\([^)]*\)/g) */

    var match = prev.match(/[^a-zA-Z0-9\._)\]}"](~?[a-zA-Z0-9\._]+)$/)
    if (!match) {
      /* var openFnCall = oprev.match(/([a-zA-Z0-9\._]+)\([^()]+$/) */
      /* console.log(openFnCall, oprev) */
      return
    }
    if (match[1][0] == '~') {
      return // TODO
    }
    var parts = match[1].split('.')
    var name = parts.pop()
    var prefix = parts.join('.')

    const opens  = findOpens(prev).reverse()
    /* oprev.replace(/\bopen\s+([A-Z][\w_]*)/g, (a, b) => opens.push(b)) */
    const openPrefixes = {}
    opens.forEach((name, i) => {
      Object.keys(openPrefixes).forEach(k => openPrefixes[k + '.' + name] = true)
      openPrefixes[name] = true
    });
    console.log('pr', openPrefixes)

    var matching = window.complationData.filter(item => {
      // TODO be case agnostic?
      if (!item.name.startsWith(name)) return false
      if (!item.path.endsWith(prefix)) {
        /* console.log('prefix', item.path, prefix) */
        return false
      }
      var left = prefix.length ? item.path.slice(0, -prefix.length) : item.path
      if (left[left.length - 1] == '.') {
        left = left.slice(0, -1)
      }
      if (left && !openPrefixes[left]) {
        /* console.log('left', left, item.path, prefix) */
        return false
      }
      return true
    })

    var node = (tag, attrs, children) => {
      var node = document.createElement(tag)
      for (var attr in attrs) {
        if (attr === 'style') {
          Object.assign(node.style, attrs[attr])
        } else {
          node.setAttribute(attr, attrs[attr])
        }
      }
      children && children.forEach(child => node.appendChild(typeof child === 'string' ? document.createTextNode(child) : child))
      return node
    }
  var raw = text => {
    var node = document.createElement('div')
    node.innerHTML = text
    return node
  };


    if (!matching.length) return
    // TODO TODO
    // this isn't resolving:
    // open Reprocessing;
    // Draw.<complete please>
    // BUT it does get
    // Reprocessing.Draw.rectf

    var colors = {
      'type': '#faa',
      'value': '#afa',
      'module': '#aaf',
    }

    const data = {
        from: {line: cur.line, ch: cur.ch - name.length},
        to: cur,
        list: matching.map(item => ({
          text: item.name,
          displayText: item.name,
          item,
          render: (elem, _, __) => {
            var container = node('span', {}, [
              node('span', {style: {
                backgroundColor: colors[item.kind] || '#eee',
                borderRadius: '50%',
                marginRight: '4px',
                padding: '0 2px',
                color: 'black',
              }}, [item.kind[0] || '']),
              item.name
            ])
            container.style.lineHeight = 1;
            elem.appendChild(container)
          }
        })).sort((a, b) => a.text.length - b.text.length)
    }
    var contents = raw('')
    var helper = node('div', {
      style: {
        position: 'absolute',
        left: '100%',
        top: 0,
        marginLeft: 4,
        fontFamily: 'iosevka, "sf pro mono", monospace',
        whiteSpace: 'pre-wrap',
        fontSize: '12px',
        lineHeight: 1.2,
        padding: '4px 8px',
        zIndex: 1000,
        backgroundColor: 'white',
        boxShadow: '0 0 2px #aaa',
      }
    }, [contents])
    CodeMirror.on(data, 'select', function(completion, element) {
      onSelect(completion.item)
      contents.innerHTML = completion.item.type
      var list = element.parentNode;
      var box = list.getBoundingClientRect()
      helper.style.left = list.style.left
      helper.style.top = list.style.top
      helper.style.marginLeft = box.width + 'px'
      element.parentNode.parentNode.appendChild(helper)
    })
    cm.showHint({
      completeSingle: false,
      hint: () => (data)
    });
    /* onSelect(data.list[0].item) */
    CodeMirror.on(data, 'close', () => {
      helper.parentNode && helper.parentNode.removeChild(helper)
      onClose()
    })
    return true
  })
|}];

let registerComplete: (codemirror, codemirror => bool) => unit = [%bs.raw{|
  (function(cm, onHint) {
    var ExcludedIntelliSenseTriggerKeys =
{
    "8": "backspace",
    "9": "tab",
    "13": "enter",
    "16": "shift",
    "17": "ctrl",
    "18": "alt",
    "19": "pause",
    "20": "capslock",
    "27": "escape",
    "33": "pageup",
    "34": "pagedown",
    "35": "end",
    "36": "home",
    "37": "left",
    "38": "up",
    "39": "right",
    "40": "down",
    "45": "insert",
    "46": "delete",
    "91": "left window key",
    "92": "right window key",
    "93": "select",
    "107": "add",
    "109": "subtract",
    "110": "decimal point",
    "111": "divide",
    "112": "f1",
    "113": "f2",
    "114": "f3",
    "115": "f4",
    "116": "f5",
    "117": "f6",
    "118": "f7",
    "119": "f8",
    "120": "f9",
    "121": "f10",
    "122": "f11",
    "123": "f12",
    "144": "numlock",
    "145": "scrolllock",
    "186": "semicolon",
    "187": "equalsign",
    "188": "comma",
    "189": "dash",
    "191": "slash",
    "192": "graveaccent",
    "220": "backslash",
    "222": "quote"
}

cm.on("keyup", function(editor, event)
{
    if (!ExcludedIntelliSenseTriggerKeys[(event.keyCode || event.which).toString()]) {
      /* if (    cm.state.completionActive && event.key != ".") return */
        if (!onHint(cm) && cm.state.completionActive) {
          cm.state.completionActive.close()
        }
    }
});
  })
|}];

let highlightNode: (. Dom.element, string) => unit = [%bs.raw {|
  (function(node, token) {
    function escapeRegExp(string) {
      return string.replace(/[.*+?^${}()|[\]\\]/g, '\\$&'); // $& means the whole matched string
    }

    var walk = (node, fn) => {
      var nodes = [].slice.call(node.childNodes)
      nodes.forEach(child => {
        if (false === fn(child)) return;
        if (child.parentNode === node) {
          walk(child, fn)
        }
      })
    }

    walk(node, node => {
      if (node.nodeName === '#text') {
        let at = 0;
        let pieces = [];
        node.textContent.replace(new RegExp(escapeRegExp(token), 'gi'), (matched, pos, full) => {
          pieces.push(document.createTextNode(full.slice(at, pos)))
          var span = document.createElement('span')
          span.textContent = matched
          span.className='result-highlighted'
          pieces.push(span)
          at = pos + matched.length
        })
        if (pieces.length === 0) {
          return
        }
        if (at < node.textContent.length) {
          pieces.push(document.createTextNode(node.textContent.slice(at)))
        }
        node.replaceWith(...pieces)
      }
    })

  })
|}];

let fromTextArea: (. textarea, string => unit) => codemirror = [%bs.raw {|
  function(textarea, onRun) {
    var betterShiftTab = /*onInfo => */cm => {
      var cursor = cm.getCursor()
        , line = cm.getLine(cursor.line)
        , pos = {line: cursor.line, ch: cursor.ch}
      cm.execCommand('indentLess')
    }

    var betterTab = /*onComplete => */cm => {
      if (cm.somethingSelected()) {
        return cm.indentSelection("add");
      }
      const cursor = cm.getCursor()
      const line = cm.getLine(cursor.line)
      const pos = {line: cursor.line, ch: cursor.ch}
      cm.replaceSelection(Array(cm.getOption("indentUnit") + 1).join(" "), "end", "+input");
    }

    var run = function(cm) {
      onRun(cm.getValue())
    }

    var toggleComment = cm => {
      var options = {indent: true}
      var minLine = Infinity, ranges = cm.listSelections(), mode = null;
      for (var i = ranges.length - 1; i >= 0; i--) {
        var from = ranges[i].from(), to = ranges[i].to();
        if (from.line >= minLine) continue;
        if (to.line >= minLine) to = Pos(minLine, 0);
        minLine = from.line;
        if (mode == null) {
          if (cm.uncomment(from, to, options)) mode = "un";
          else { cm.blockComment(from, to, options); mode = "line"; }
        } else if (mode == "un") {
          cm.uncomment(from, to, options);
        } else {
          cm.blockComment(from, to, options);
        }
      }
    };

    var cm = CodeMirror.fromTextArea(textarea, {
      lineNumbers: true,
      lineWrapping: true,
      viewportMargin: Infinity,
      extraKeys: {
        /* extraKeys: {"Ctrl-Space": "autocomplete"}, */
        'Cmd-Enter': (cm) => onRun(cm.getValue()),
        'Ctrl-Enter': (cm) => onRun(cm.getValue()),
        "Cmd-/": toggleComment,
        Tab: betterTab,
        'Shift-Tab': betterShiftTab,
      },
      mode: 'rust',
    })
    return cm
  }
|}];


let htmlEscape = text => text
|> Js.String.replace("&", "&amp;")
|> Js.String.replace("<", "&lt;")
|> Js.String.replace(">", "&gt;")
;

let fixEscapes = message => message
|> Js.String.replaceByRe(Js.Re.fromStringWithFlags({js|\u001b\\[1;|js}, ~flags="g"), {js|\u001b[|js})
|> Js.String.replaceByRe(Js.Re.fromStringWithFlags({js|\u001b\\[0m|js}, ~flags="g"), {js|\u001b[39m|js})
|> parseAnsi
|> result => result##spans
|> Array.map(span => {
  let css = span##css;
  let text = htmlEscape(span##text);
  Js.log2("span", span);
  {j|<span style="$css">$text</span>|j}
}) |> Js.Array.joinWith("");

type pos = {
  row: int,
  column: int
};
let jsPos = ({row, column}) => {"line": row, "ch": column};


type result('a, 'b) =
  | Ok('a)
  | Error('b);

let reasonCompile = code => {
  let result = reason_compile(code);
  switch (Js.Nullable.toOption(result##js_code)) {
  | Some(js) => Ok(js)
  | None =>
    Error((
      result##js_error_msg,
      {row: result##row, column: result##column},
      {row: result##endRow, column: result##endColumn}
    ))
  };
};

let ocamlCompile = code => {
  let result = ocaml_compile(code);
  switch (Js.Nullable.toOption(result##js_code)) {
  | Some(js) => Ok(js)
  | None =>
    Error((
      result##js_error_msg,
      {row: result##row, column: result##column},
      {row: result##endRow, column: result##endColumn}
    ))
  };
};