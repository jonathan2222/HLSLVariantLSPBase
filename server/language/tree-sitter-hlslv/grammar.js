/**
 * @file Hlslvparser grammar for tree-sitter
 * @author Jonathan Åleskog
 * @license MIT
 */

/// <reference types="tree-sitter-cli/dsl" />
// @ts-check

module.exports = grammar({
  name: "hlslvparser",

  rules: {
    // TODO: add the actual grammar rules
    source_file: $ => "hello"
  }
});
