import MarkdownIt from 'markdown-it'
// @ts-ignore
import attr from 'markdown-it-link-attributes'
import Token from 'markdown-it/lib/token'

export function useMarkdown() {
  const md = new MarkdownIt({
    html: true,
    typographer: true,
    linkify: true,
  })

  const defaultRender = md.renderer.rules.link_open || function (tokens, idx, options, env, self) {
    return self.renderToken(tokens, idx, options)
  }

  md.renderer.rules.link_open = function (tokens, idx, options, env, self) {
    const token = tokens[idx]
    const hrefIndex = token.attrIndex('href')

    if (hrefIndex >= 0) {
      const href = token.attrs?.[hrefIndex][1]

      if (!isValidLink(href)) {
        const spanToken = new Token('span_open', 'span', 1)
        spanToken.block = false
        tokens[idx] = spanToken

        for (let i = idx + 1; i < tokens.length; i++) {
          if (tokens[i].type === 'link_close') {
            const spanCloseToken = new Token('span_close', 'span', -1)
            spanCloseToken.block = false
            tokens[i] = spanCloseToken
            break
          }
        }
      }
    }

    return defaultRender(tokens, idx, options, env, self)
  }

  function isValidLink(href: string | undefined) {
    if (!href) return false
    try {
      const url = new URL(href)
      if (url.protocol !== 'http:' && url.protocol !== 'https:') return false
      if (url.hostname === 'localhost') return false
      return true
    } catch {
      return false
    }
  }

  md.use(attr, {
    attrs: {
      target: 'browser',
      rel: 'noopener noreferrer',
    },
  })

  // A "badge paragraph" is a <p> whose only meaningful content is one or more
  // link-wrapped images (`<a><img/></a>`) — i.e. shields.io / download.fo /
  // similar shield-style buttons. Plain screenshots (bare <img>) and inline
  // images mixed with text are intentionally left alone.
  function isBadgeParagraph(p: Element): boolean {
    let hasBadge = false
    for (const node of Array.from(p.childNodes)) {
      if (node.nodeType === Node.TEXT_NODE) {
        if ((node.textContent || '').trim() !== '') return false
        continue
      }
      if (node.nodeType !== Node.ELEMENT_NODE) return false
      const el = node as Element
      // <br> separators between badges are tolerable
      if (el.tagName === 'BR') continue
      if (el.tagName !== 'A') return false
      if (el.children.length !== 1) return false
      if (el.children[0].tagName !== 'IMG') return false
      hasBadge = true
    }
    return hasBadge
  }

  // Group consecutive badge paragraphs into a single horizontal flex row.
  function groupBadgeParagraphs(html: string): string {
    if (typeof DOMParser === 'undefined') return html

    const doc = new DOMParser().parseFromString(`<div id="root">${html}</div>`, 'text/html')
    const root = doc.getElementById('root')
    if (!root) return html

    const children = Array.from(root.children)
    let i = 0
    while (i < children.length) {
      const el = children[i]
      if (el.tagName === 'P' && isBadgeParagraph(el)) {
        let j = i + 1
        while (j < children.length && children[j].tagName === 'P' && isBadgeParagraph(children[j])) {
          j++
        }
        const wrapper = doc.createElement('div')
        wrapper.className = 'md-badge-row'
        root.insertBefore(wrapper, el)
        for (let k = i; k < j; k++) {
          const p = children[k]
          for (const child of Array.from(p.childNodes)) {
            if (child.nodeType === Node.ELEMENT_NODE && (child as Element).tagName === 'A') {
              wrapper.appendChild(child)
            }
          }
          p.remove()
        }
        i = j
      } else {
        i++
      }
    }

    return root.innerHTML
  }

  const render = (t: string) => groupBadgeParagraphs(md.render(t))

  return {
    render,
  }
}
