// Generated by BUCKLESCRIPT VERSION 2.1.0, PLEASE EDIT WITH CARE
'use strict';

var Fs                      = require("fs");
var List                    = require("bs-platform/lib/js/list.js");
var Block                   = require("bs-platform/lib/js/block.js");
var Json_decode             = require("bs-json/src/Json_decode.js");
var Caml_builtin_exceptions = require("bs-platform/lib/js/caml_builtin_exceptions.js");
var Render$LonaCompilerCore = require("./render.bs.js");

function parseFile(filename) {
  var content = Fs.readFileSync(filename, "utf8");
  var parsed = JSON.parse(content);
  var parseColor = function (json) {
    return /* record */[
            /* id */Json_decode.field("id", Json_decode.string, json),
            /* name */Json_decode.field("name", Json_decode.string, json),
            /* value */Json_decode.field("value", Json_decode.string, json)
          ];
  };
  return Json_decode.field("colors", (function (param) {
                return Json_decode.list(parseColor, param);
              }), parsed);
}

function find(colors, id) {
  var exit = 0;
  var color;
  try {
    color = List.find((function (color) {
            return +(color[/* id */0] === id);
          }), colors);
    exit = 1;
  }
  catch (exn){
    if (exn === Caml_builtin_exceptions.not_found) {
      return /* None */0;
    } else {
      throw exn;
    }
  }
  if (exit === 1) {
    return /* Some */[color];
  }
  
}

function render(target, colors) {
  if (target !== 0) {
    var colorConstantDoc = function (color) {
      return /* LineEndComment */Block.__(16, [{
                  comment: color[/* value */2],
                  line: /* ConstantDeclaration */Block.__(6, [{
                        modifiers: /* :: */[
                          /* AccessLevelModifier */Block.__(0, [/* PublicModifier */3]),
                          /* :: */[
                            /* StaticModifier */11,
                            /* [] */0
                          ]
                        ],
                        pattern: /* IdentifierPattern */Block.__(0, [{
                              identifier: color[/* id */0],
                              annotation: /* None */0
                            }]),
                        init: /* Some */[/* LiteralExpression */Block.__(0, [/* Color */Block.__(4, [color[/* value */2]])])]
                      }])
                }]);
    };
    return Render$LonaCompilerCore.Swift[/* toString */9](/* TopLevelDeclaration */Block.__(19, [{
                    statements: /* :: */[
                      /* ImportDeclaration */Block.__(10, ["UIKit"]),
                      /* :: */[
                        /* Empty */0,
                        /* :: */[
                          /* ClassDeclaration */Block.__(4, [{
                                name: "Colors",
                                inherits: /* [] */0,
                                modifier: /* None */0,
                                isFinal: /* false */0,
                                body: List.map(colorConstantDoc, colors)
                              }]),
                          /* [] */0
                        ]
                      ]
                    ]
                  }]));
  } else {
    console.log("Color generation not supported for target", target);
    return "error";
  }
}

exports.parseFile = parseFile;
exports.find      = find;
exports.render    = render;
/* fs Not a pure module */
