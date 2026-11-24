// Usage: node exodus/atomizer/native-provider-cli.mjs graph.json
//        node exodus/atomizer/native-provider-cli.mjs --selftest
import {readFileSync} from "node:fs";
import {mapProviders,selftest} from "./native-providers.mjs";

try {
  if (process.argv.length !== 3) throw new Error("expected graph.json or --selftest");
  if (process.argv[2] === "--selftest") {
    console.log(selftest() + " assertions passed");
  } else {
    const graph = JSON.parse(readFileSync(process.argv[2], "utf8"));
    console.log(JSON.stringify(mapProviders(graph), null, 2));
  }
} catch (error) {
  console.error(error.message);
  process.exitCode = 1;
}
