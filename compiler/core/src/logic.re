type logicValue =
  | Identifier(Types.lonaType, list(string))
  | Literal(Types.lonaValue)
  | None;

type logicNode =
  | If(logicValue, Types.cmp, logicValue, logicNode)
  | IfExists(logicValue, logicNode)
  | Assign(logicValue, logicValue)
  | Add(logicValue, logicValue, logicValue)
  | Let(logicValue)
  | Block(list(logicNode))
  | None;

module IdentifierSet = {
  include
    Set.Make(
      {
        type t = (Types.lonaType, list(string));
        let compare = (a: t, b: t) : int => {
          let (_, a) = a;
          let (_, b) = b;
          compare(Render.String.join("", a), Render.String.join("", b))
        };
      }
    );
};

module LogicTree =
  Tree.Make(
    {
      type t = logicNode;
      let children = (node) =>
        switch node {
        | If(_, _, _, value) => [value]
        | Add(_, _, _) => []
        | Assign(_, _) => []
        | IfExists(_, value) => [value]
        | Block(body) => body
        | Let(_) => []
        | None => []
        };
      let restore = (node, contents) => {
        let at = (index) => List.nth(contents, index);
        switch node {
        | If(a, b, c, _) => If(a, b, c, at(0))
        | Add(_, _, _) => node
        | Assign(_, _) => node
        | IfExists(a, _) => IfExists(a, at(0))
        | Block(_) => Block(contents)
        | Let(_) => node
        | None => node
        }
      };
    }
  );

/* TODO: This only looks at assignments */
let undeclaredIdentifiers = (node) => {
  let inner = (node, identifiers) =>
    switch node {
    | Assign(_, Identifier(type_, path)) => IdentifierSet.add((type_, path), identifiers)
    | _ => identifiers
    };
  LogicTree.reduce(inner, IdentifierSet.empty, node)
};

let assignedIdentifiers = (node) => {
  let inner = (node, identifiers) =>
    switch node {
    | Assign(_, Identifier(type_, path)) => IdentifierSet.add((type_, path), identifiers)
    | _ => identifiers
    };
  LogicTree.reduce(inner, IdentifierSet.empty, node)
};

let conditionallyAssignedIdentifiers = (rootNode) => {
  let identifiers = undeclaredIdentifiers(rootNode);
  let paths = identifiers |> IdentifierSet.elements;
  let rec isAlwaysAssigned = (target, node) =>
    switch node {
    | Assign(_, Identifier(_, path)) => path == target
    | If(_, _, Identifier(_, path), body) when path == target => isAlwaysAssigned(target, body)
    | Block(nodes) => nodes |> List.exists(isAlwaysAssigned(target))
    | _ => false
    };
  let accumulate = (set, (ltype, path)) =>
    isAlwaysAssigned(path, rootNode) ? set : IdentifierSet.add((ltype, path), set);
  paths |> List.fold_left(accumulate, IdentifierSet.empty)
};

/* let testNode = Assign(Identifier(Reference("OK"), ["a"]), Identifier(Reference("OK"), ["b"])); */
let addVariableDeclarations = (node) => {
  let identifiers = undeclaredIdentifiers(node);
  identifiers
  |> IdentifierSet.elements
  |> List.map(((type_, path)) => Let(Identifier(type_, path)))
  |> List.fold_left(
       (acc, declaration) =>
         LogicTree.insert_child((item) => item == acc ? Some(declaration) : None, acc),
       node
     )
};

let logicValueToJavaScriptAST = (x) =>
  switch x {
  | Identifier(_, path) => Ast.JavaScript.Identifier(path)
  | Literal(x) => Literal(x)
  | None => Unknown
  };

let rec toJavaScriptAST = (node) => {
  let fromCmp = (x) =>
    switch x {
    | Types.Eq => Ast.JavaScript.Eq
    | Neq => Neq
    | Gt => Gt
    | Gte => Gte
    | Lt => Lt
    | Lte => Lte
    | Unknown => Noop
    };
  switch node {
  | Assign(a, b) =>
    Ast.JavaScript.AssignmentExpression(logicValueToJavaScriptAST(b), logicValueToJavaScriptAST(a))
  | IfExists(a, body) =>
    ConditionalStatement(logicValueToJavaScriptAST(a), [toJavaScriptAST(body)])
  | Block(body) => Ast.JavaScript.Block(body |> List.map(toJavaScriptAST))
  | If(a, cmp, b, body) =>
    let condition =
      Ast.JavaScript.BooleanExpression(
        logicValueToJavaScriptAST(a),
        fromCmp(cmp),
        logicValueToJavaScriptAST(b)
      );
    ConditionalStatement(condition, [toJavaScriptAST(body)])
  | Add(lhs, rhs, value) =>
    let addition =
      Ast.JavaScript.BooleanExpression(
        logicValueToJavaScriptAST(lhs),
        Plus,
        logicValueToJavaScriptAST(rhs)
      );
    AssignmentExpression(logicValueToJavaScriptAST(value), addition)
  | Let(value) =>
    switch value {
    | Identifier(_, path) => Ast.JavaScript.VariableDeclaration(Ast.JavaScript.Identifier(path))
    | _ => Unknown
    }
  | None => Unknown
  }
};

let rec toSwiftAST = (colors, rootLayer: Types.layer, logicRootNode) => {
  let identifierName = (node) =>
    switch node {
    | Identifier(ltype, [head, ...tail]) =>
      switch head {
      | "parameters" => Ast.Swift.SwiftIdentifier(List.hd(tail))
      | "layers" =>
        switch tail {
        | [second, ...tail] when second == rootLayer.name =>
          Ast.Swift.SwiftIdentifier(
            List.tl(tail)
            |> List.fold_left((a, b) => a ++ "." ++ Swift.Format.camelCase(b), List.hd(tail))
          )
        | [second, ...tail] =>
          Ast.Swift.SwiftIdentifier(
            tail
            |> List.fold_left(
                 (a, b) => a ++ "." ++ Swift.Format.camelCase(b),
                 Swift.Format.layerName(second)
               )
          )
        | _ => SwiftIdentifier("BadIdentifier")
        }
      | _ => SwiftIdentifier("BadIdentifier")
      }
    | _ => SwiftIdentifier("BadIdentifier")
    };
  let logicValueToSwiftAST = (x) =>
    switch x {
    | Identifier(_) => identifierName(x)
    | Literal(value) => Swift.Document.lonaValue(colors, value)
    | None => Empty
    };
  let typeAnnotationDoc =
    fun
    | Types.Reference(typeName) =>
      switch typeName {
      | "Boolean" => Ast.Swift.TypeName("Bool")
      | _ => TypeName(typeName)
      }
    | Named(name, _) => TypeName(name);
  let fromCmp = (x) =>
    switch x {
    | Types.Eq => "=="
    | Neq => "!="
    | Gt => ">"
    | Gte => ">="
    | Lt => "<"
    | Lte => "<="
    | Unknown => "???"
    };
  let unwrapBlock =
    fun
    | Block(body) => body
    | node => [node];
  let rec inner = (logicRootNode) =>
    switch logicRootNode {
    | Assign(a, b) =>
      let (left, right) =
        switch (logicValueToSwiftAST(b), logicValueToSwiftAST(a)) {
        | (Ast.Swift.SwiftIdentifier(name), LiteralExpression(Boolean(value)))
            when name |> Js.String.endsWith("visible") => (
            Ast.Swift.SwiftIdentifier(name |> Js.String.replace("visible", "isHidden")),
            Ast.Swift.LiteralExpression(Boolean(! value))
          )
        | (Ast.Swift.SwiftIdentifier(name), right) when name |> Js.String.endsWith("borderRadius") => (
            Ast.Swift.SwiftIdentifier(
              name |> Js.String.replace("borderRadius", "layer.cornerRadius")
            ),
            right
          )
        | (Ast.Swift.SwiftIdentifier(name), right) when name |> Js.String.endsWith("height") => (
            Ast.Swift.SwiftIdentifier(
              name |> Js.String.replace(".height", "HeightAnchorConstraint?.constant")
            ),
            right
          )
        | (Ast.Swift.SwiftIdentifier(name), right) when name |> Js.String.endsWith("width") => (
            Ast.Swift.SwiftIdentifier(
              name |> Js.String.replace(".width", "WidthAnchorConstraint?.constant")
            ),
            right
          )
        | nodes => nodes
        };
      Ast.Swift.BinaryExpression({"left": left, "operator": "=", "right": right})
    | IfExists(a, body) =>
      /* TODO: Once we support optional params, compare to nil or extract via pattern */
      Ast.Swift.IfStatement({
        "condition": logicValueToSwiftAST(a),
        "block": unwrapBlock(body) |> List.map(inner)
      })
    | Block(body) => Ast.Swift.StatementListHelper(body |> List.map(inner))
    | If(a, cmp, b, body) =>
      Ast.Swift.IfStatement({
        "condition":
          Ast.Swift.BinaryExpression({
            "left": logicValueToSwiftAST(a),
            "operator": fromCmp(cmp),
            "right": logicValueToSwiftAST(b)
          }),
        "block": unwrapBlock(body) |> List.map(inner)
      })
    | Add(lhs, rhs, value) =>
      BinaryExpression({
        "left": logicValueToSwiftAST(value),
        "operator": "=",
        "right":
          Ast.Swift.BinaryExpression({
            "left": logicValueToSwiftAST(lhs),
            "operator": "+",
            "right": logicValueToSwiftAST(rhs)
          })
      })
    | Let(value) =>
      switch value {
      | Identifier(ltype, path) =>
        Ast.Swift.VariableDeclaration({
          "modifiers": [],
          "pattern":
            Ast.Swift.IdentifierPattern({
              "identifier": List.fold_left((a, b) => a ++ "." ++ b, List.hd(path), List.tl(path)),
              "annotation": Some(ltype |> typeAnnotationDoc)
            }),
          "init": (None: option(Ast.Swift.node)),
          "block": (None: option(Ast.Swift.initializerBlock))
        })
      | _ => Empty
      }
    | None => Empty
    };
  logicRootNode |> unwrapBlock |> List.map(inner)
};