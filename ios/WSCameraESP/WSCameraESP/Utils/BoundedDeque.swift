//
//  BoundedDeque.swift
//  WSCameraESP
//
//  Created by Sam Meech-Ward on 2025-01-20.
//


import DequeModule

struct BoundedDeque<Element> {
    private var deque: Deque<Element> = []
    private let maxSize: Int

    init(maxSize: Int) {
        self.maxSize = maxSize
    }

    mutating func add(_ item: Element) {
        deque.append(item)
        if deque.count > maxSize {
            deque.removeFirst()
        }
    }

    func allItems() -> [Element] {
        return Array(deque)
    }
}
