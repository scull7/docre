
let unwrap = (m, x) => switch x { | None => failwith(m) | Some(x) => x };

let annotateSourceCode = (source, cmt, mlast, output) => {
  let annots = Cmt_format.read_cmt(cmt).Cmt_format.cmt_annots;
  let (types, bindings, externals, all_opens, locToPath) = Typing.collectTypes(annots);
  let text = Files.readFile(source) |> unwrap("Unable to read source file");
  let structure = ReadMlast.structure(mlast);
  let (highlighted, typeText) = Highlighting.highlight(text, structure, types, bindings, externals, all_opens, locToPath);
  Files.writeFile(output, Template.make(highlighted, typeText)) |> ignore;
};

let getName = x => Filename.basename(x) |> Filename.chop_extension;

let processCmt = (name, cmt) => {
  let annots = Cmt_format.read_cmt(cmt).Cmt_format.cmt_annots;

  switch annots {
  | Cmt_format.Implementation({str_items} as s) => {
    /* Printtyped.implementation(Format.str_formatter, s);
    let out = Format.flush_str_formatter();
    Files.writeFile("debug_" ++ name ++ ".typ.inft", out) |> ignore; */

    let stamps = CmtFindStamps.stampsFromTypedtreeImplementation((name, []), str_items);
    let (topdoc, allDocs) = CmtFindDocItems.docItemsFromStructure(str_items);
    (name, cmt, stamps, topdoc, allDocs)
  }
  | Cmt_format.Interface({sig_items} as s) => {
    /* Printtyped.interface(Format.str_formatter, s);
    let out = Format.flush_str_formatter();
    Files.writeFile("debug_" ++ name ++ ".typ.inft", out) |> ignore; */

    let stamps = CmtFindStamps.stampsFromTypedtreeInterface((name, []), sig_items);
    let (topdoc, allDocs) = CmtFindDocItems.docItemsFromSignature(sig_items);
    (name, cmt, stamps, topdoc, allDocs)
  }
  | _ => failwith("Not a valid cmt file")
  };
};

open Infix;

let filterDuplicates = cmts => {
  /* Remove .cmt's that have .cmti's */
  let intfs = Hashtbl.create(100);
  cmts |> List.iter(path => if (Filename.check_suffix(path, ".cmti")) {
    Hashtbl.add(intfs, getName(path), true)
  });
  cmts |> List.filter(path => {
    !(Filename.check_suffix(path, ".cmt") && Hashtbl.mem(intfs, getName(path)))
  });
};

let sliceToEnd = (s, num) => String.length(s) < num ? s : String.sub(s, num, String.length(s) - num);
let slice = (s, pre, post) => String.sub(s, pre, String.length(s) - pre + post);

let startsWith = (prefix, string) => {
  let lp = String.length(prefix);
  lp <= String.length(string) && String.sub(string, 0, lp) == prefix
};
open Infix;

let generateMultiple = (~bsRoot, ~editingEnabled, ~test, base, compiledBase, url, dest, cmts, markdowns: list((string, option(string), Omd.t, string))) => {
  Files.mkdirp(dest);

  let cmts = filterDuplicates(cmts);

  let cssLoc = Filename.concat(dest, "styles.css");
  let jsLoc = Filename.concat(dest, "script.js");
  let allDeps = Filename.concat(dest, "all-deps.js");

  Files.writeFile(cssLoc, DocsTemplate.styles) |> ignore;
  Files.writeFile(jsLoc, DocsTemplate.script) |> ignore;

  let names = List.map(getName, cmts);

  let searchHref = (names, doc) => {
    switch (Docs.formatHref("", names, doc)) {
    | None => None
    /* | Some(href) when href != "" && href.[0] == '#' => Some(href) */
    | Some(href) => Some("./api/" ++ href)
    }
  };

  let processedCmts = cmts |> List.map(cmt => processCmt(getName(cmt), cmt));

  let codeBlocksOverride = CodeSnippets.process(~bsRoot, ~editingEnabled, ~test, markdowns, processedCmts, base, dest);

  let (searchables, processDocString) = Markdown.makeDocStringProcessor(dest, codeBlocksOverride);


  let api = dest /+ "api";
  Files.mkdirp(api);
  processedCmts |> List.iter(((name, cmt, stamps, topdoc, allDocs)) => {
    let name = getName(cmt);
    let output = dest /+ "api" /+ name ++ ".html";
    let rel = Files.relpath(Filename.dirname(output));

    let markdowns = List.map(((path, source, _contents, name)) => (rel(path), name), markdowns);

    /* let (stamps, topdoc, allDocs) = processCmt(name, cmt); */
    let searchPrinter = GenerateDoc.printer(searchHref(names), stamps);
    let sourceUrl = url |?> (url => {
      let relative = Files.relpath(compiledBase, cmt) |> Filename.chop_extension;
      let isInterface = Filename.check_suffix(cmt, "i");
      let re = Filename.concat(base, relative) ++ (isInterface ? ".rei" : ".re");
      let ml = Filename.concat(base, relative) ++ (isInterface ? ".mli" : ".ml");
      if (Files.exists(re)) {
        Some(url ++ relative ++ (isInterface ? ".rei" : ".re"))
      } else if (Files.exists(ml)) {
        Some(url ++ relative ++ (isInterface ? ".mli" : ".ml"))
      } else {
        None
      }
    });
    let text = Docs.generate(~sourceUrl, ~relativeToRoot=rel(dest), ~cssLoc=Some(rel(cssLoc)), ~jsLoc=Some(rel(jsLoc)), ~processDocString=processDocString(searchPrinter, output, name), name, topdoc, stamps, allDocs, names, markdowns);

    Files.writeFile(output, text) |> ignore;
  });

  (markdowns) |> List.iter(((path, source, contents, name)) => {
    let rel = Files.relpath(Filename.dirname(path));
    let (tocItems, override) = GenerateDoc.trackToc(~lower=true, 0, Markdown.linkifyMarkdown(path, dest));
    let searchPrinter = GenerateDoc.printer(searchHref(names), []);
    let main = processDocString(searchPrinter, path, name, ~override, [], name, None, contents);

    let sourceUrl = url |?> (url => {
      source |?>> (source => {
        let relative = Files.relpath(dest, source);
        url /+ Files.relpath(base, dest) /+ relative
      })
    });

    let markdowns = List.map(((path, _source, _contents, name)) => (rel(path), name), markdowns);
    let projectListing = names |> List.map(name => (rel(api /+ name ++ ".html"), name));
    let html = Docs.page(~sourceUrl, ~relativeToRoot=rel(dest), ~cssLoc=Some(rel(cssLoc)), ~jsLoc=Some(rel(jsLoc)), name, List.rev(tocItems^), projectListing, markdowns, main);

    Files.writeFile(path, html) |> ignore;
  });

  {
    let path = dest /+ "search.html";
    let rel = Files.relpath(Filename.dirname(path));
    let markdowns = List.map(((path, source, contents, name)) => (rel(path), name), markdowns);
    let projectListing = names |> List.map(name => (rel(api /+ name ++ ".html"), name));
    let main = Printf.sprintf({|
      <input placeholder="Search the docs" id="search-input"/>
      <style>%s</style>
      <div id="search-results"></div>
      <link rel=stylesheet href="search.css">
      <script defer src="searchables.json.index.js"></script>
      <script defer src="elasticlunr.js"></script>
      <script defer src="search.js"></script>
    |}, DocsTemplate.searchStyle);
    let html = Docs.page(~sourceUrl=None, ~relativeToRoot=rel(dest), ~cssLoc=Some(rel(cssLoc)), ~jsLoc=Some(rel(jsLoc)), "Search", [], projectListing, markdowns, main);
    Files.writeFile(path, html) |> ignore;
    Files.writeFile(dest /+ "search.js", SearchScript.js) |> ignore;
    Files.writeFile(dest /+ "elasticlunr.js", ElasticRaw.raw) |> ignore;
    Files.writeFile(dest /+ "searchables.json", Search.serializeSearchables(searchables^)) |> ignore;
    MakeIndex.run(dest /+ "elasticlunr.js", dest /+ "searchables.json")
  };
};

let unwrap = (m, n) => switch n { | None => failwith(m) | Some(n) => n };
let optOr = (d, o) => switch o { | None => d | Some(n) => n };

let stripNumber = name => {
  if (name.[0] <= '9' && name.[0] >= '0' && name.[1] == '_') {
    String.sub(name, 2, String.length(name) - 2)
  } else {
    name
  }
};

/** TODO use this somewhere */
let escapePath = path => Str.global_replace(Str.regexp("[^a-zA-Z0-9_.-]"), "-", path);

let asHtml = path => Filename.chop_extension(path) ++ ".html";

let isReadme = path => Filename.check_suffix(String.lowercase(path), "/readme.md");

let htmlName = path => {
  if (isReadme(path)) {
    String.sub(path, 0, String.length(path) - String.length("/readme.md")) /+ "index.html"
  } else {
    asHtml(path)
  }
};

let getOrder = path => {
  if (isReadme(path)) {
    ""
  } else {
    path
  }
};

let getTitle = (path, base) => {
  if (isReadme(path)) {
    let dir = Filename.dirname(path);
    if (dir == base) {
      "Home"
    } else {
      Filename.basename(dir)
    }
  } else {
    getName(path) |> stripNumber
  }
};

let getMarkdowns = (projectName, base, target) => {
  let files = Files.collect(target, name => Filename.check_suffix(name, ".md"));
  let files = files |> List.map(path => {
    (getOrder(path), htmlName(path), Some(path), Files.readFile(path) |> unwrap("Unable to read markdown file " ++ path), getTitle(path, target))
  });
  let files = if (!List.exists(((_, path, _, _, _)) => String.lowercase(path) == String.lowercase(target) /+ "readme.md", files)) {
    let readme = base /+ "Readme.md";
    switch (Files.readDirectory(base) |> List.find(name => String.lowercase(name) == "readme.md")) {
    | exception Not_found => {
      [("", target /+ "index.html", None,  "# " ++ projectName ++ "\n\nWelcome to the documentation!", "Home"), ...files]
    }
    | name => {
      let readme = base /+ name;
      let contents = Files.readFile(readme) |! "Unable to read " ++ readme;
      [("", target /+ "index.html", Some(readme), contents, "Home"), ...files]
    }
    }
  } else {
    files
  };
  List.sort(compare, files) |> List.map(((_, html, source, contents, name)) => (html, source, Omd.of_string(contents), name))
};

let isCmt = name => {
  !startsWith(Filename.basename(name), CodeSnippets.codeBlockPrefix) && (Filename.check_suffix(name, ".cmt") || Filename.check_suffix(name, ".cmti"));
};

let getBsbVersion = base => {
  let (out, success) = Commands.execSync(base /+ "node_modules/.bin/bsb -version");
  if (!success) {
    "2.2.3"
  } else {
    let out = String.concat("", out) |> String.trim;
    out
  }
};

let generateProject = (~selfPath, ~projectName, ~root, ~target, ~sourceDirectories, ~test, ~bsRoot) => {
  Files.mkdirp(target);
  let bsConfig = Json.parse(Files.readFile(root /+ "bsconfig.json") |! "No bsconfig.json found");
  let (found, compiledRoot) = sourceDirectories == [] ? {
    let sourceDirectories = CodeSnippets.getSourceDirectories(root, bsConfig);
    let isNative = CodeSnippets.isNative(bsConfig);
    let compiledRoot = root /+ (isNative ? "lib/bs/js" : "lib/bs");
    let found = sourceDirectories |> List.map(name => compiledRoot /+ name) |> List.map(p => Files.collect(p, isCmt)) |> List.concat;
    (found, compiledRoot)
    /* HACKKKK */
  } : (sourceDirectories |> List.map(p => Files.collect(p, isCmt)) |> List.concat, List.hd(sourceDirectories));


  let markdowns = getMarkdowns(projectName, root, target);
  let url = ParseConfig.getUrl(root);

  let static = Filename.dirname(selfPath) /+ "../../../static";
  let bsbVersion = getBsbVersion(root);
  let bsbFile = static /+ "bs-" ++ bsbVersion ++ ".js";
  let editingEnabled = Files.exists(bsbFile);
  if (editingEnabled) {
    Files.copy(~source=bsbFile, ~dest=target /+ "bucklescript.js") |> ignore;
  } else {
    print_endline("No bucklescript file available -- editing will be disabled")
  };

  generateMultiple(~bsRoot, ~editingEnabled, ~test, root, compiledRoot, url, target, found, markdowns);

  [
  "block-script.js",
  ...editingEnabled ? [
  "jsx-ppx.js",
  "refmt.js",
  "codemirror-5.36.0/lib/codemirror.js",
  "codemirror-5.36.0/lib/codemirror.css",
  "codemirror-5.36.0/mode/rust/rust.js",
  "codemirror-5.36.0/addon/mode/simple.js",
  ] : []]
  |> List.iter(name => {
    Files.copy(~source=static /+ name, ~dest=target /+ Filename.basename(name)) |> ignore;
  });

  let localUrl = "file://" ++ Files.absify(target) /+ "index.html";
  print_newline();
  print_endline("Complete! Docs are available in " ++ target ++ "\nOpen " ++ localUrl ++ " in your browser to view");
  print_newline();
};

let parse = Minimist.parse(
  ~alias=[("h", "help"), ("test", "doctest")],
  ~presence=["help", "doctest"],
  ~multi=["cmi-directory"],
  ~strings=["target", "root", "name", "bs-root"]
);

let help = {|
# docre - a clean & easy documentation generator

Usage: docre [options]

  --root (default: current directory)
      expected to contain bsconfig.json, and bs-platform in the node_modules
  --target (default: {root}/docs)
      where we should write out the docs
  --name (default: the name of the directory, capitalized)
      what this project is called
  --cmi-directory
  --bs-root (default: root/node_modules/bs-platform)
  --doctest (default: false)
      execute the documentation snippets to make sure they run w/o erroring
  -h, --help
      print this help
|};

let fail = (msg) => {
  print_endline(msg);
  print_endline(help);
  exit(1);
};

let (selfPath, args) = switch (Array.to_list(Sys.argv)) {
| [] => { print_endline(help); exit(0); }
| [one, ...rest] => (one, rest)
};
