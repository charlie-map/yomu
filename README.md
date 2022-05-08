# Meet Yomu!
### Charlie Hall

**Yomu** is a fully open-source C library for parsing, viewing, and changing given HTML, XML, or any other markup language (with similar semantics). **Yomu** is designed for simplicty and uses a similar style as [jQuery](https://jquery.com/) (with less functionality currently). Initially built for [Wikipedia Suggestion Service (Wikiread)](https://github.com/charlie-map/wiki-suggestor-service), this library expands on that for general purposes. Below is an example of parsing a simple HTML string into a **Yomu** structure:

```C
yomu_f.init();
yomu_t *yomu = yomu_f.parse("<div>Some random, but <span>special</span> example</div>");

char *yomuParsedContent = yomu_f.read(yomu, "");

printf("The data in the example: %s\n", yomuParsedContent);

free(yomuParsedContent);

yomu_f.destroy(yomu);
yomu_f.close();
```
