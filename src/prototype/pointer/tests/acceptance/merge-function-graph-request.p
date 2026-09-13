// Investigative fixture: use merge-function-graph-composition.p as --imports.
// Ordinary execution passes; graph declaration formation is still unsupported.
import curriedMerge;
graph := @curriedMerge;
